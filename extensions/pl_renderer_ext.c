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
#include "pl_renderer_internal.c"

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
    pl_log_debug_f(gptLog, gptData->uLogChannel, "created directional light: '%s'", pcName);
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
    pl_log_debug_f(gptLog, gptData->uLogChannel, "created point light: '%s'", pcName);
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
    pl_log_debug_f(gptLog, gptData->uLogChannel, "created environment probe: '%s'", pcName);
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
    pl_log_debug_f(gptLog, gptData->uLogChannel, "created spot light: '%s'", pcName);
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
    pl_log_debug_f(gptLog, gptData->uLogChannel, "created object: '%s'", pcName);
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
    pl_log_debug_f(gptLog, gptData->uLogChannel, "copied object: '%s'", pcName);

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
pl_renderer_create_skin(plComponentLibrary* ptLibrary, const char* pcName, plSkinComponent** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed skin";
    pl_log_debug_f(gptLog, gptData->uLogChannel, "created skin: '%s'", pcName);
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

    gptStage->initialize((plStageInit){.ptDevice = gptData->ptDevice});

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
    gptData->tRuntimeOptions.bNormalMapping = true;
    gptData->tRuntimeOptions.fShadowConstantDepthBias = -1.25f;
    gptData->tRuntimeOptions.fShadowSlopeDepthBias = -10.75f;
    gptData->tRuntimeOptions.fExposure = 1.0f;
    gptData->tRuntimeOptions.fBrightness = 0.0f;
    gptData->tRuntimeOptions.fContrast = 1.0f;
    gptData->tRuntimeOptions.fSaturation = 1.0f;
    gptData->tRuntimeOptions.tTonemapMode = PL_TONEMAP_MODE_KHRONOS_PBR_NEUTRAL;
    gptData->tRuntimeOptions.bBloomActive = false;
    gptData->tRuntimeOptions.fBloomRadius = 1.5f;
    gptData->tRuntimeOptions.fBloomStrength = 0.05f;
    gptData->tRuntimeOptions.uBloomChainLength = 5;

    gptData->tRuntimeOptions.fGridCellSize = 0.025f;
    gptData->tRuntimeOptions.fGridMinPixelsBetweenCells = 2.0f;
    gptData->tRuntimeOptions.tGridColorThin = (plVec4){0.5f, 0.5f, 0.5f, 1.0f};
    gptData->tRuntimeOptions.tGridColorThick = (plVec4){0.75f, 0.75f, 0.75f, 1.0f};

    gptData->tRuntimeOptions.fFogDensity = 0.1f;
    gptData->tRuntimeOptions.fFogHeight = 0.0f;
    gptData->tRuntimeOptions.fFogStart = 1.0f;
    gptData->tRuntimeOptions.fFogCutOffDistance = 1000.0f;
    gptData->tRuntimeOptions.fFogMaxOpacity = 0.1f;
    gptData->tRuntimeOptions.fFogHeightFalloff = 0.1f;
    gptData->tRuntimeOptions.tFogColor = (plVec3){1.0f, 1.0f, 1.0f};

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

    gptData->tViewBGLayout = gptShaderVariant->get_bind_group_layout("view");
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
    const plSamplerDesc tSamplerLinearClampDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .tUAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .tMipmapMode     = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .pcDebugName     = "linear clamp"
    };
    gptData->tSamplerLinearClamp = gptGfx->create_sampler(gptData->ptDevice, &tSamplerLinearClampDesc);

    const plSamplerDesc tSamplerNearestClampDesc = {
        .tMagFilter      = PL_FILTER_NEAREST,
        .tMinFilter      = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .tUAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .tMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "nearest clamp"
    };
    gptData->tSamplerNearestClamp = gptGfx->create_sampler(gptData->ptDevice, &tSamplerNearestClampDesc);

    const plSamplerDesc tSamplerLinearRepeatDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "linear repeat"
    };
    gptData->tSamplerLinearRepeat = gptGfx->create_sampler(gptData->ptDevice, &tSamplerLinearRepeatDesc);

    const plSamplerDesc tSamplerNearestRepeatDesc = {
        .tMagFilter      = PL_FILTER_NEAREST,
        .tMinFilter      = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "nearest repeat"
    };
    gptData->tSamplerNearestRepeat = gptGfx->create_sampler(gptData->ptDevice, &tSamplerNearestRepeatDesc);

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
            }
        }
    };
    gptData->tRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tRenderPassLayoutDesc);

    const plRenderPassLayoutDesc tTransparentRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true },  // depth buffer
            { .tFormat = PL_FORMAT_R16G16B16A16_FLOAT }, // final output
        },
        .atSubpasses = {
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
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
                .tDestinationAccessMask = PL_ACCESS_INPUT_ATTACHMENT_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
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
    gptData->tTransparentRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tTransparentRenderPassLayoutDesc);

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
            { .tFormat = PL_FORMAT_R16G16B16A16_FLOAT },
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0},
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

    // create post processing render pass
    const plRenderPassLayoutDesc tFinalRenderPassLayoutDesc = {
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
    gptData->tFinalRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tFinalRenderPassLayoutDesc);

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

    // create semaphores
    gptData->ptClickSemaphore = gptGfx->create_semaphore(gptData->ptDevice, false);
};

plScene*
pl_renderer_create_scene(plSceneInit tInit)
{

    if(tInit.szIndexBufferSize == 0)    tInit.szIndexBufferSize = 64000000;
    if(tInit.szVertexBufferSize == 0)   tInit.szVertexBufferSize = 64000000;
    if(tInit.szDataBufferSize == 0)     tInit.szDataBufferSize = 64000000;
    if(tInit.szMaterialBufferSize == 0) tInit.szMaterialBufferSize = 8000000;

    plScene* ptScene = PL_ALLOC(sizeof(plScene));
    memset(ptScene, 0, sizeof(plScene));
    pl_sb_push(gptData->sbptScenes, ptScene);

    ptScene->ptComponentLibrary = tInit.ptComponentLibrary;
    ptScene->pcName = "unnamed scene";
    ptScene->bActive = true;
    ptScene->bProbeCountDirty = true;
    gptFreeList->create((uint64_t)tInit.szMaterialBufferSize, sizeof(plGpuMaterial), &ptScene->tMaterialFreeList);

    // create global bindgroup
    ptScene->uTextureIndexCount = 0;

    plBindGroupLayoutHandle tGlobalSceneBindGroupLayout = gptShaderVariant->get_bind_group_layout("scene");

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // create global bindgroup
        const plBindGroupDesc tGlobalBindGroupDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = tGlobalSceneBindGroupLayout,
            .pcDebugName = "global bind group"
        };
        ptScene->atBindGroups[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBindGroupDesc);
    }

    const plBindGroupDesc tSkinBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("skinning", 0),
        .pcDebugName = "skin bind group"
    };
    ptScene->tSkinBindGroup0 = gptGfx->create_bind_group(gptData->ptDevice, &tSkinBindGroupDesc);

    const plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = tInit.szIndexBufferSize,
        .pcDebugName = "index buffer"
    };

    const plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = tInit.szVertexBufferSize,
        .pcDebugName = "vertex buffer"
    };

    const plBufferDesc tStorageBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = tInit.szDataBufferSize,
        .pcDebugName = "storage buffer"
    };

    ptScene->tIndexBuffer = pl__renderer_create_local_buffer(&tIndexBufferDesc, "index", 0);
    ptScene->tVertexBuffer = pl__renderer_create_local_buffer(&tVertexBufferDesc, "vertex", 0);
    ptScene->tStorageBuffer = pl__renderer_create_local_buffer(&tStorageBufferDesc, "vertex data", 0);

    gptFreeList->create(tIndexBufferDesc.szByteSize, 128, &ptScene->tIndexBufferFreeList);
    gptFreeList->create(tVertexBufferDesc.szByteSize, 128, &ptScene->tVertexBufferFreeList);
    gptFreeList->create(tStorageBufferDesc.szByteSize, 128, &ptScene->tStorageBufferFreeList);

    // pre-create some global buffers
    const plBufferDesc tMaterialDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize = tInit.szMaterialBufferSize,
        .pcDebugName = "material buffer"
    };
    ptScene->tMaterialDataBuffer = pl__renderer_create_local_buffer(&tMaterialDataBufferDesc,  "material buffer", 0);

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = PL_MAX_LIGHTS * sizeof(plGpuLightShadow),
        .pcDebugName = "shadow data buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    const plBufferDesc atProbeDataBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = 4096,
        .pcDebugName = "probe buffers"
    };

    const plBufferDesc tSceneBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plGpuSceneData),
        .pcDebugName = "scene buffer"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        ptScene->atSceneBuffer[i]           = pl__renderer_create_staging_buffer(&tSceneBufferDesc, "scene buffer", i);
        ptScene->atLightShadowDataBuffer[i] = pl__renderer_create_staging_buffer(&atLightShadowDataBufferDesc, "shadow buffer", i);
        ptScene->atShadowCameraBuffers[i]   = pl__renderer_create_staging_buffer(&atCameraBuffersDesc, "shadow camera buffer", i);
        ptScene->atGPUProbeDataBuffers[i]   = pl__renderer_create_staging_buffer(&atProbeDataBufferDesc, "probe buffer", i);
    }

    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        { .uSlot = 0, .tBuffer = ptScene->tStorageBuffer, .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tStorageBuffer)->tDesc.szByteSize},
        { .uSlot = 1, .tBuffer = ptScene->tVertexBuffer,  .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tVertexBuffer)->tDesc.szByteSize},
    };

    const plBindGroupUpdateSamplerData atSamplerData[] = {
        { .uSlot = 3, .tSampler = gptData->tSamplerLinearRepeat}
    };

    plBindGroupUpdateData tBGData0 = {
        .uBufferCount = 2,
        .atBufferBindings = atBufferData,
        .uSamplerCount = 1,
        .atSamplerBindings = atSamplerData,
    };
    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tSkinBindGroup0, &tBGData0);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

        // partially update global bindgroup (just samplers)
        plBindGroupUpdateSamplerData tGlobalSamplerData[] = {
            { .uSlot = 4, .tSampler = gptData->tSamplerLinearClamp },
            { .uSlot = 5, .tSampler = gptData->tSamplerNearestClamp },
            { .uSlot = 6, .tSampler = gptData->tSamplerLinearRepeat },
            { .uSlot = 7, .tSampler = gptData->tSamplerNearestRepeat }
        };

        plBindGroupUpdateBufferData tGlobalBufferData[] = {
            { .uSlot = 0, .tBuffer = ptScene->atSceneBuffer[i], .szBufferRange = sizeof(plGpuSceneData) },
            {
                .tBuffer       = ptScene->tStorageBuffer,
                .uSlot         = 1,
                .szBufferRange = gptGfx->get_buffer(gptData->ptDevice, ptScene->tStorageBuffer)->tDesc.szByteSize
            }
        };


        plBindGroupUpdateData tGlobalBindGroupData = {
            .uSamplerCount     = 4,
            .atSamplerBindings = tGlobalSamplerData,
            .uBufferCount = 2,
            .atBufferBindings = tGlobalBufferData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atBindGroups[i], &tGlobalBindGroupData);
    }

    // pre-create working buffers for environment filtering, later we should defer this
    for(uint32_t i = 0; i < 7; i++)
    {
        const size_t uMaxFaceSize = ((size_t)1024 * (size_t)1024) * 4 * sizeof(float);

        const plBufferDesc tInputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER_SOURCE,
            .szByteSize = uMaxFaceSize,
            .pcDebugName = "filter buffers"
        };
        ptScene->atFilterWorkingBuffers[i] = pl__renderer_create_local_buffer(&tInputBufferDesc, "filter buffer", i);
    }

    // create probe material & mesh
    plMaterialComponent* ptMaterial = NULL;
    plEntity tMaterial = gptMaterial->create(ptScene->ptComponentLibrary, "environment probe material", &ptMaterial);
    ptMaterial->tAlphaMode = PL_MATERIAL_ALPHA_MODE_OPAQUE;
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tFlags |= PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW;
    ptMaterial->tFlags &= ~PL_MATERIAL_FLAG_CAST_SHADOW;
    ptMaterial->tBaseColor = (plVec4){1.0f, 1.0f, 1.0f, 1.0f};
    ptMaterial->fRoughness = 0.0f;
    ptMaterial->fMetalness = 1.0f;
    pl_renderer__add_material_to_scene(ptScene, tMaterial);

    plMeshComponent* ptMesh = NULL;
    ptScene->tProbeMesh = gptMesh->create_sphere_mesh(ptScene->ptComponentLibrary, "environment probe mesh", 0.25f, 32, 32, &ptMesh);
    ptMesh->tMaterial = tMaterial;

    ptMesh = NULL;
    ptScene->tUnitSphereMesh = gptMesh->create_sphere_mesh(ptScene->ptComponentLibrary, "unit sphere mesh", 1.0f, 16, 16, &ptMesh);

    // const uint32_t uStartIndex = pl_sb_size(ptScene->sbtVertexPosBuffer);
    // const uint32_t uIndexStart = pl_sb_size(ptScene->sbuIndexBuffer);


    // pl_sb_resize(ptScene->sbuIndexBuffer, (uint32_t)ptMesh->szIndexCount);
    // pl_sb_resize(ptScene->sbtVertexPosBuffer, (uint32_t)ptMesh->szVertexCount);
    // memcpy(&ptScene->sbtVertexPosBuffer[uStartIndex], ptMesh->ptVertexPositions, sizeof(plVec3) * ptMesh->szVertexCount);
    // memcpy(&ptScene->sbuIndexBuffer[uIndexStart], ptMesh->puIndices, sizeof(uint32_t) * ptMesh->szIndexCount);

    plFreeListNode* ptIndexBufferNode = gptFreeList->get_node(&ptScene->tIndexBufferFreeList, (uint32_t)ptMesh->szIndexCount * sizeof(uint32_t));
    plFreeListNode* ptVertexBufferNode = gptFreeList->get_node(&ptScene->tVertexBufferFreeList, (uint32_t)ptMesh->szVertexCount * sizeof(plVec3));

    const plDrawable tDrawable = {
        .uIndexCount     = (uint32_t)ptMesh->szIndexCount,
        .uVertexCount    = (uint32_t)ptMesh->szVertexCount,
        .uIndexOffset    = (uint32_t)(ptIndexBufferNode->uOffset / sizeof(uint32_t)),
        .uVertexOffset   = (uint32_t)(ptVertexBufferNode->uOffset / sizeof(plVec3)),
        .uTransformIndex = ptScene->uNextTransformIndex++,
        .uTriangleCount  = (uint32_t)ptMesh->szIndexCount / 3
    };
    ptScene->tUnitSphereDrawable = tDrawable;

    gptStage->stage_buffer_upload(ptScene->tIndexBuffer, ptIndexBufferNode->uOffset, ptMesh->puIndices, sizeof(uint32_t) * ptMesh->szIndexCount);
    gptStage->stage_buffer_upload(ptScene->tVertexBuffer, ptVertexBufferNode->uOffset, ptMesh->ptVertexPositions, sizeof(plVec3) * ptMesh->szVertexCount);
    gptStage->flush();

    pl_sb_reset(ptScene->sbuIndexBuffer);
    pl_sb_reset(ptScene->sbtVertexPosBuffer);

    // create shadow atlas
    ptScene->uShadowAtlasResolution = 1024 * 8 * 2;
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
        .tDimensions = {.x = (float)ptScene->uShadowAtlasResolution, .y = (float)ptScene->uShadowAtlasResolution},
        .pcDebugName = "Secondary Shadow"
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
        .tDimensions = {.x = (float)ptScene->uShadowAtlasResolution, .y = (float)ptScene->uShadowAtlasResolution},
        .pcDebugName = "First Shadow"
    };

    plRenderPassAttachments atShadowAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    ptScene->atShadowTextureBindlessIndices = pl__renderer_get_bindless_texture_index(ptScene, ptScene->tShadowTexture);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        atShadowAttachmentSets[i].atViewAttachments[0] = ptScene->tShadowTexture;
    }
    ptScene->tShadowRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tDepthRenderPassDesc, atShadowAttachmentSets);
    ptScene->tFirstShadowRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tFirstDepthRenderPassDesc, atShadowAttachmentSets);

    const plTextureDesc tLutTextureDesc = {
        .tDimensions = {(float)1024, (float)1024, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    ptScene->tBrdfLutTexture = pl__renderer_create_texture(&tLutTextureDesc, "lut texture", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptScene->tSceneData.iBrdfLutIndex = pl__renderer_get_bindless_texture_index(ptScene, ptScene->tBrdfLutTexture);

    const plBindGroupDesc tBrdfBGSet1Desc = {
        .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("brdf_lut", 0),
        .pcDebugName = "brdf_lut_set_1"
    };
    plBindGroupHandle tBrdfBGSet1 = gptGfx->create_bind_group(gptData->ptDevice, &tBrdfBGSet1Desc);
    
    const size_t uFaceSize = ((size_t)1024 * (size_t)1024) * 4 * sizeof(float);

    const plBindGroupUpdateBufferData tBrdfBGSet1BufferData[] = {
        { .uSlot = 0, .tBuffer = ptScene->atFilterWorkingBuffers[0], .szBufferRange = uFaceSize}
    };

    const plBindGroupUpdateData tBrdfBGSet1Data = {
        .uBufferCount = 1,
        .atBufferBindings = tBrdfBGSet1BufferData,
    };
    gptGfx->update_bind_group(gptData->ptDevice, tBrdfBGSet1, &tBrdfBGSet1Data);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, tBrdfBGSet1);

    plComputeShaderHandle tBrdfLutShader = gptShaderVariant->get_compute_shader("brdf_lut", NULL);
    
    const plDispatch tDispach0 = {
        .uGroupCountX     = (uint32_t)1024 / 16,
        .uGroupCountY     = (uint32_t)1024 / 16,
        .uGroupCountZ     = 1,
        .uThreadPerGroupX = 16,
        .uThreadPerGroupY = 16,
        .uThreadPerGroupZ = 1
    };

    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 2");
    const plBeginCommandInfo tBeginInfo0 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {tSemHandle},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo0);

    const plPassBufferResource atPassBuffers[] = {
        { .tHandle = ptScene->atFilterWorkingBuffers[0], .tStages = PL_SHADER_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
    };

    const plPassResources tPassResources = {
        .uBufferCount = 1,
        .atBuffers = atPassBuffers
    };

    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);

    plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(gptData->ptDevice);
    plGpuDynFilterSpec* ptDynamicData = (plGpuDynFilterSpec*)tDynamicBinding.pcData;
    ptDynamicData->iResolution = 1024;
    ptDynamicData->fRoughness = 0.0f;
    ptDynamicData->iSampleCount = (int)1024;
    ptDynamicData->iWidth = 0;
    ptDynamicData->fLodBias = 0.0f;
    ptDynamicData->iCurrentMipLevel = 0;

    plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
    gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);
    gptGfx->bind_compute_bind_groups(ptComputeEncoder, tBrdfLutShader, 0, 1, &tBrdfBGSet1, 1, &tDynamicBinding);
    gptGfx->bind_compute_shader(ptComputeEncoder, tBrdfLutShader);
    gptGfx->dispatch(ptComputeEncoder, 1, &tDispach0);

    gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ);
    gptGfx->end_compute_pass(ptComputeEncoder);

    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tSubmitInfo0 = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {tSemHandle},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo0);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "env cube 3");
    const plBeginCommandInfo tBeginInfo1 = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {tSemHandle},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo1);
    plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    const plBufferImageCopy tBufferImageCopy0 = {
        .uImageWidth = 1024,
        .uImageHeight = 1024,
        .uImageDepth = 1,
        .uLayerCount = 1,
        .szBufferOffset = 0,
        .uBaseArrayLayer = 0,
    };
    gptGfx->copy_buffer_to_texture(ptBlitEncoder, ptScene->atFilterWorkingBuffers[0], ptScene->tBrdfLutTexture, 1, &tBufferImageCopy0);
    gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlitEncoder);
    gptGfx->end_command_recording(ptCommandBuffer);
    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {tSemHandle},
        .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
    };
    gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~GPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBufferDesc tLightBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_UNIFORM,
        .szByteSize = sizeof(plGpuLight) * PL_MAX_LIGHTS,
        .pcDebugName = "light buffer"
    };

    const plBufferDesc tTransformBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plMat4) * 10000,
        .pcDebugName = "transform buffer"
    };

    const plBufferDesc tInstanceBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plShadowInstanceBufferData) * 10000,
        .pcDebugName = "instance buffer"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        ptScene->atLightBuffer[i]     = pl__renderer_create_staging_buffer(&tLightBufferDesc, "light", i);
        ptScene->atTransformBuffer[i] = pl__renderer_create_staging_buffer(&tTransformBufferDesc, "transform", i);
        ptScene->atInstanceBuffer[i]  = pl__renderer_create_staging_buffer(&tInstanceBufferDesc, "instance", i);
    }

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(gptData->tRuntimeOptions.bPunctualLighting)   iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting) iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;
    if(gptData->tRuntimeOptions.bNormalMapping)      iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_NORMAL_MAPS;
    if(gptData->tRuntimeOptions.bPcfShadows)         iSceneWideRenderingFlags |= PL_RENDERING_FLAG_PCF_SHADOWS;

    // create lighting shader
    int aiLightingConstantData[] = {iSceneWideRenderingFlags, gptData->tRuntimeOptions.tShaderDebugMode, 0};
    ptScene->tDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_directional", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tSpotLightingShader = gptShaderVariant->get_shader("deferred_lighting_spot", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tPointLightingShader = gptShaderVariant->get_shader("deferred_lighting_point", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    aiLightingConstantData[0] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
    aiLightingConstantData[2] = 0;
    ptScene->tEnvDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_directional", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tEnvSpotLightingShader = gptShaderVariant->get_shader("deferred_lighting_spot", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tEnvPointLightingShader = gptShaderVariant->get_shader("deferred_lighting_point", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupUpdateBufferData atGlobalBufferData[] = 
        {
            {
                .tBuffer       = ptScene->atTransformBuffer[i],
                .uSlot         = 2,
                .szBufferRange = sizeof(plMat4) * 10000
            },
            {
                .tBuffer       = ptScene->tMaterialDataBuffer,
                .uSlot         = 3,
                .szBufferRange = tInit.szMaterialBufferSize
            },
        };

        plBindGroupUpdateData tGlobalBindGroupData = {
            .uBufferCount = 2,
            .atBufferBindings = atGlobalBufferData,
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atBindGroups[i], &tGlobalBindGroupData);
    }

    return ptScene;
}

void
pl_renderer_cleanup_view(plView* ptView)
{

    pl_sb_free(ptView->sbtVisibleDrawables);
    pl_sb_free(ptView->sbuVisibleDeferredEntities);
    pl_sb_free(ptView->sbuVisibleForwardEntities);
    pl_sb_free(ptView->sbuVisibleTransmissionEntities);
    pl_sb_free(ptView->tDirectionLightShadowData.sbtDLightShadowData);

    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tAlbedoTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tNormalTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tAOMetalRoughnessTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tRawOutputTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tDepthTexture);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atUVMaskTexture0);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->atUVMaskTexture1);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tFinalTexture);
    if(ptView->ptParentScene->bTransmissionRequired)
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tTransmissionTexture);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tPostProcessRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tPickRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tUVRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptView->tFinalRenderPass);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->tFinalTextureHandle);
    gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptView->tLightingBindGroup);

    for(uint32_t i = 0; i < pl_sb_size(ptView->sbtBloomDownChain); i++)
    {
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomDownChain[i]);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomUpChain[i]);
    }
    pl_sb_free(ptView->sbtBloomDownChain);
    pl_sb_free(ptView->sbtBloomUpChain);
    
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->tPickTexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptView->atViewBuffers[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptView->atView2Buffers[i]);
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

    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tBrdfLutTexture);
    for(uint32_t j = 0; j < pl_sb_size(ptScene->sbtProbeData); j++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[j];
            
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tLambertianEnvTexture);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tGGXEnvTexture);
        if(ptScene->bSheenRequired)
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptProbe->tSheenEnvTexture);
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
            gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptProbe->atViewBuffers[i]);
            gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptProbe->atView2Buffers[i]);
            gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptProbe->tDirectionLightShadowData.atDLightShadowDataBuffer[i]);
            gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptProbe->tDirectionLightShadowData.atDShadowCameraBuffers[i]);
        }

        for(uint32_t k = 0; k < 6; k++)
        {
            pl_sb_free(ptProbe->sbuVisibleDeferredEntities[k]);
            pl_sb_free(ptProbe->sbuVisibleForwardEntities[k]);
            pl_sb_free(ptProbe->sbuVisibleTransmissionEntities[k]);
        }
        pl_sb_free(ptProbe->tDirectionLightShadowData.sbtDLightShadowData);
    }

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atSceneBuffer[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atLightBuffer[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atShadowCameraBuffers[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atLightShadowDataBuffer[i]);
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atGPUProbeDataBuffers[i]);
        gptGfx->queue_bind_group_for_deletion(gptData->ptDevice, ptScene->atBindGroups[i]);
    }
    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tMaterialDataBuffer);
    
    for(uint32_t i = 0; i < 7; i++)
    {
        gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->atFilterWorkingBuffers[i]);
    }

    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tVertexBuffer);
    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tIndexBuffer);
    gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->tStorageBuffer);
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtSkinData); i++)
    {
        for(uint32_t j = 0; j < gptGfx->get_frames_in_flight(); j++)
        {
            if(gptGfx->is_buffer_valid(gptData->ptDevice, ptScene->sbtSkinData[i].atDynamicSkinBuffer[j]))
                gptGfx->queue_buffer_for_deletion(gptData->ptDevice, ptScene->sbtSkinData[i].atDynamicSkinBuffer[j]);
        }
            
    }

    // gptTerrain->cleanup_terrain(ptScene->ptTerrain);
    gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptScene->tShadowTexture);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptScene->tShadowRenderPass);
    gptGfx->queue_render_pass_for_deletion(gptData->ptDevice, ptScene->tFirstShadowRenderPass);

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
    pl_sb_free(ptScene->sbtShadowRectData);
    pl_sb_free(ptScene->sbtLights);
    pl_sb_free(ptScene->sbuShadowDeferredDrawables);
    pl_sb_free(ptScene->sbuShadowForwardDrawables);
    pl_sb_free(ptScene->sbtLightShadowData);
    pl_sb_free(ptScene->sbtLightData);
    pl_sb_free(ptScene->sbtVertexPosBuffer);
    pl_sb_free(ptScene->sbtVertexDataBuffer);
    pl_sb_free(ptScene->sbuIndexBuffer);
    pl_sb_free(ptScene->sbtMaterialNodes)
    pl_sb_free(ptScene->sbtDrawables);
    pl_sb_free(ptScene->sbtDrawableResources);
    pl_sb_free(ptScene->sbtSkinData);
    pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
    pl_sb_free(ptScene->sbtOutlinedEntities);
    pl_sb_free(ptScene->sbtOutlineDrawablesOldShaders);
    pl_sb_free(ptScene->sbtOutlineDrawablesOldEnvShaders);
    pl_hm_free(&ptScene->tDrawableHashmap);
    pl_hm_free(&ptScene->tMaterialHashmap);
    pl_hm_free(&ptScene->tTextureIndexHashmap);
    pl_hm_free(&ptScene->tCubeTextureIndexHashmap);


    gptFreeList->cleanup(&ptScene->tMaterialFreeList);
    gptFreeList->cleanup(&ptScene->tIndexBufferFreeList);
    gptFreeList->cleanup(&ptScene->tVertexBufferFreeList);
    gptFreeList->cleanup(&ptScene->tStorageBufferFreeList);

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

    ptView->uIndex = pl_sb_size(ptScene->sbptViews);
    pl_sb_push(ptScene->sbptViews, ptView);

    ptView->ptParentScene = ptScene;
    ptView->tTargetSize = tDimensions;
    ptView->tData.tViewportSize.xy = tDimensions;

    // picking defaults
    ptView->tHoveredEntity.uData = 0;
    ptView->bRequestHoverCheck = false;

    plVec3 tNewDimensions = {ptView->tTargetSize.x + 300.0f, ptView->tTargetSize.y + 300.0f, 1};
    // plVec3 tNewDimensions = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1};

    // create offscreen per-frame resources
    const plTextureDesc tRawOutputTextureDesc = {
        .tDimensions   = tNewDimensions,
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName   = "offscreen final"
    };

    const plTextureDesc tPickTextureDesc = {
        .tDimensions   = tNewDimensions,
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "pick original"
    };

    const plTextureDesc tNormalTextureDesc = {
        .tDimensions   = tNewDimensions,
        .tFormat       = PL_FORMAT_R16G16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = tNewDimensions,
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "albedo texture"
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = tNewDimensions,
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "offscreen depth texture"
    };

    const plTextureDesc tMaskTextureDesc = {
        .tDimensions   = tNewDimensions,
        .tFormat       = PL_FORMAT_R32G32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
        .pcDebugName   = "mask texture"
    };

    const plTextureDesc tEmmissiveTexDesc = {
        .tDimensions   = tNewDimensions,
        .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "emissive texture"
    };

    const plBufferDesc atView2BuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = 4096,
        .pcDebugName = "view buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = PL_MAX_LIGHTS * sizeof(plGpuLightShadow),
        .pcDebugName = "shadow data buffer"
    };

    const plBufferDesc atViewBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_UNIFORM,
        .szByteSize = sizeof(plGpuViewData),
        .pcDebugName = "view buffer"
    };

    // create offscreen render pass
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atFinalAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
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
    ptView->tFinalTextureHandle      = gptDraw->create_bind_group_for_texture(ptView->tFinalTexture);

    const plBindGroupDesc tJFABindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptShaderVariant->get_compute_bind_group_layout("jumpfloodalgo", 0),
        .pcDebugName = "temp jfa bind group"
    };

    ptView->atJFABindGroups[0] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);
    ptView->atJFABindGroups[1] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);

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

    gptGfx->update_bind_group(gptData->ptDevice, ptView->atJFABindGroups[0], &tJFABGData0);
    gptGfx->update_bind_group(gptData->ptDevice, ptView->atJFABindGroups[1], &tJFABGData1);

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
            .tUsage     = PL_BUFFER_USAGE_STORAGE,
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
        ptView->atView2Buffers[i] = pl__renderer_create_staging_buffer(&atView2BuffersDesc, "scene", i);
        
        const plBindGroupDesc tDeferredBG1Desc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = gptShaderVariant->get_graphics_bind_group_layout("gbuffer_fill", 1),
            .pcDebugName = "view specific bindgroup"
        };

        const plBindGroupUpdateBufferData tView2BufferBGData = {
            .tBuffer       = ptView->atView2Buffers[i],
            .uSlot         = 0,
            .szBufferRange = sizeof(plGpuViewData)
        };

        plBindGroupUpdateData tView2BGData = {
            .uBufferCount    = 1,
            .atBufferBindings = &tView2BufferBGData
        };
        
        ptView->atDeferredBG1[i] = gptGfx->create_bind_group(gptData->ptDevice, &tDeferredBG1Desc);
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atDeferredBG1[i], &tView2BGData);

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
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tFinalTexture;
        
        atFinalAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atFinalAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture;

        ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[i] = pl__renderer_create_staging_buffer(&atLightShadowDataBufferDesc, "d shadow", i);
        ptView->tDirectionLightShadowData.atDShadowCameraBuffers[i] = pl__renderer_create_staging_buffer(&atCameraBuffersDesc, "d shadow buffer", i);
        ptView->atViewBuffers[i] = pl__renderer_create_staging_buffer(&atViewBuffersDesc, "view buffer", i);

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
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
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
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y},
        .pcDebugName = "Main"
    };
    ptView->tRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tRenderPassDesc, atAttachmentSets);

    const plRenderPassDesc tTransparentRenderPassDesc = {
        .tLayout = gptData->tTransparentRenderPassLayout,
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
                .tLoadOp       = PL_LOAD_OP_LOAD,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y},
        .pcDebugName = "Main 2"
    };
    ptView->tTransparentRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tTransparentRenderPassDesc, atAttachmentSets);

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
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y},
        .pcDebugName = "Pick"
    };
    ptView->tPickRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tPickRenderPassDesc, atPickAttachmentSets);

    const plRenderPassDesc tPostProcessRenderPassDesc = {
        .tLayout = gptData->tPostProcessRenderPassLayout,
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y},
        .pcDebugName = "JFA"
    };
    ptView->tPostProcessRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tPostProcessRenderPassDesc, atPostProcessAttachmentSets);

    const plRenderPassDesc tFinalRenderPassDesc = {
        .tLayout = gptData->tFinalRenderPassLayout,
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
                .tLoadOp       = PL_LOAD_OP_LOAD,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage    = PL_TEXTURE_USAGE_SAMPLED,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y},
        .pcDebugName = "Final"
    };
    ptView->tFinalRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tFinalRenderPassDesc, atFinalAttachmentSets);

    // register debug 3D drawlist
    ptView->pt3DDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DGizmoDrawList = gptDraw->request_3d_drawlist();
    ptView->pt3DSelectionDrawList = gptDraw->request_3d_drawlist();

    // create offscreen renderpass
    const plRenderPassDesc tUVRenderPass0Desc = {
        .tLayout = gptData->tUVRenderPassLayout,
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
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE,
                .tNextUsage    = PL_TEXTURE_USAGE_STORAGE,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 0.0f}
            }
        },
        .tDimensions = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y},
        .pcDebugName = "UV"
    };
    ptView->tUVRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tUVRenderPass0Desc, atUVAttachmentSets);

    return ptView;
}

plVec2
pl_renderer_get_view_color_texture_max_uv(plView* ptView)
{
    plTexture* ptTexture = gptGfx->get_texture(gptData->ptDevice, ptView->tFinalTexture);
    return (plVec2){ptView->tTargetSize.x / ptTexture->tDesc.tDimensions.x, ptView->tTargetSize.y / ptTexture->tDesc.tDimensions.y};
}

void
pl_renderer_resize_view(plView* ptView, plVec2 tDimensions)
{

    plTexture* ptTexture = gptGfx->get_texture(gptData->ptDevice, ptView->tFinalTexture);

    // update offscreen size to match viewport
    ptView->tTargetSize = tDimensions;
    ptView->tData.tViewportSize.xy = ptView->tTargetSize;

    for(uint32_t i = 0; i < pl_sb_size(ptView->sbtBloomDownChain); i++)
    {
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomDownChain[i]);
        gptGfx->queue_texture_for_deletion(gptData->ptDevice, ptView->sbtBloomUpChain[i]);
    }
    pl_sb_reset(ptView->sbtBloomDownChain);
    pl_sb_reset(ptView->sbtBloomUpChain);
    
    // update offscreen render pass attachments
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atFinalAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    // for convience
    plDevice* ptDevice = gptData->ptDevice;

    if(tDimensions.x > ptTexture->tDesc.tDimensions.x || tDimensions.y > ptTexture->tDesc.tDimensions.y)
    {

        plVec3 tNewDimensions = {ptView->tTargetSize.x + 400.0f, ptView->tTargetSize.y + 400.0f, 1};

        // recreate offscreen color & depth textures
        const plTextureDesc tRawOutputTextureDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE
        };

        const plTextureDesc tRawOutput2TextureDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
            .uLayers       = 1,
            .uMips         = 0,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE
        };

        const plTextureDesc tPickTextureDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT
        };

        const plTextureDesc tNormalTextureDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_R16G16_FLOAT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
            .pcDebugName   = "g-buffer normal"
        };

        const plTextureDesc tAlbedoTextureDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
        };

        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
        };

        const plTextureDesc tMaskTextureDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_R32G32_FLOAT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE
        };

        const plTextureDesc tEmmissiveTexDesc = {
            .tDimensions   = tNewDimensions,
            .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT
        };



        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tFinalTexture);
        
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture0);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture1);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tRawOutputTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAOMetalRoughnessTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickTexture);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptView->tLightingBindGroup);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptView->atJFABindGroups[0]);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptView->atJFABindGroups[1]);
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
        ptView->tFinalTextureHandle      = gptDraw->create_bind_group_for_texture(ptView->tFinalTexture);

        if(ptView->ptParentScene->bTransmissionRequired)
        {
            gptGfx->queue_texture_for_deletion(ptDevice, ptView->tTransmissionTexture);
            ptView->tTransmissionTexture = pl__renderer_create_texture(&tRawOutput2TextureDesc,  "transmission offscreen", 0, PL_TEXTURE_USAGE_SAMPLED);
            const plBindGroupUpdateTextureData tGlobalTextureData[] = {
                {
                    .tTexture = ptView->tTransmissionTexture,
                    .uSlot    = PL_MAX_BINDLESS_TEXTURE_SLOT,
                    .uIndex   = (uint32_t)ptView->tData.iTransmissionFrameBufferIndex,
                    .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
                },
            };

            plBindGroupUpdateData tGlobalBindGroupData = {
                .uTextureCount = 1,
                .atTextureBindings = tGlobalTextureData
            };

            for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
                gptGfx->update_bind_group(gptData->ptDevice, ptView->ptParentScene->atBindGroups[i], &tGlobalBindGroupData);
        }



        const plBindGroupDesc tJFABindGroupDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = gptShaderVariant->get_compute_bind_group_layout("jumpfloodalgo", 0),
            .pcDebugName = "temp jfa bind group"
        };

        ptView->atJFABindGroups[0] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);
        ptView->atJFABindGroups[1] = gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc);

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

        gptGfx->update_bind_group(gptData->ptDevice, ptView->atJFABindGroups[0], &tJFABGData0);
        gptGfx->update_bind_group(gptData->ptDevice, ptView->atJFABindGroups[1], &tJFABGData1);

        // lighting bind group
        const plBindGroupDesc tLightingBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .tLayout = gptShaderVariant->get_graphics_bind_group_layout("deferred_lighting", 2),
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
    }

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
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tFinalTexture;

        atFinalAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atFinalAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture;

        atUVAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture;
        atUVAttachmentSets[i].atViewAttachments[1] = ptView->atUVMaskTexture0;
    }
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tRenderPass, ptView->tTargetSize, atAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tTransparentRenderPass, ptView->tTargetSize, atAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tPostProcessRenderPass, ptView->tTargetSize, atPostProcessAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tPickRenderPass, ptView->tTargetSize, atPickAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tUVRenderPass, ptView->tTargetSize, atUVAttachmentSets);
    gptGfx->update_render_pass_attachments(ptDevice, ptView->tFinalRenderPass, ptView->tTargetSize, atFinalAttachmentSets);
}

void
pl_renderer_cleanup(void)
{
    pl_temp_allocator_free(&gptData->tTempAllocator);
    gptGfx->cleanup_draw_stream(&gptData->tDrawStream);

    pl_sb_free(gptData->sbptScenes);
    gptShaderVariant->unload_manifest("/shaders/shaders.pls");
    gptResource->cleanup();
    gptStage->cleanup();
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
    float* pfPanoramaData = gptImage->load_hdr_from_file(pcPath, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
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
            .tUsage     = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uPanoramaSize,
            .pcDebugName = "panorama input buffer"
        };
        atComputeBuffers[0] = pl__renderer_create_staging_buffer(&tInputBufferDesc, "panorama input", 0);
        plBuffer* ptComputeBuffer = gptGfx->get_buffer(ptDevice, atComputeBuffers[0]);
        memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, uPanoramaSize);
        
        gptImage->free(pfPanoramaData);

        const plBufferDesc tOutputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_TRANSFER_SOURCE,
            .szByteSize = uFaceSize,
            .pcDebugName = "panorama output buffer"
        };
        
        for(uint32_t i = 0; i < 6; i++)
            atComputeBuffers[i + 1] = pl__renderer_create_local_buffer(&tOutputBufferDesc, "panorama output", i);

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
        
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "load skybox 0");
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

        const plTextureDesc tSkyboxTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tSkyboxTexture = pl__renderer_create_texture(&tSkyboxTextureDesc, "skybox texture", 0, PL_TEXTURE_USAGE_SAMPLED);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "load skybox 1");
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
            .tLayout = gptShaderVariant->get_graphics_bind_group_layout("skybox", 2),
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

    const plEcsTypeKey tMaterialComponentType = gptMaterial->get_ecs_type_key();

    int iSceneWideRenderingFlags = 0;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;
    if(gptData->tRuntimeOptions.bNormalMapping)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_NORMAL_MAPS;

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
        plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, tMaterialComponentType, ptMesh->tMaterial);

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
        int aiForwardFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->tFlags,
            iObjectRenderingFlags,
            pl_sb_size(ptScene->sbtLightData),
            pl_sb_size(ptScene->sbtProbeData),
            gptData->tRuntimeOptions.tShaderDebugMode
        };

        int aiGBufferFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->tFlags,
            gptData->tRuntimeOptions.tShaderDebugMode,
            iObjectRenderingFlags
        };

        int aiVertexConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride
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

            if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
                ptDrawable->tShader = gptShaderVariant->get_shader("transmission", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, NULL);
            else if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_FORWARD)
                ptDrawable->tShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, NULL);
            else if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                ptDrawable->tShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);

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

    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
    const plEcsTypeKey tMaterialComponentType = gptMaterial->get_ecs_type_key();

    gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_CYAN, 1.0f, "%s", "reloaded shaders");

    gptShaderVariant->unload_manifest("/shaders/shaders.pls");
    gptShaderVariant->load_manifest("/shaders/shaders.pls");
    gptData->tViewBGLayout = gptShaderVariant->get_bind_group_layout("view");
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
    pl_sb_reset(ptScene->sbuShadowForwardDrawables);
    pl_sb_reset(ptScene->sbuShadowDeferredDrawables);

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(gptData->tRuntimeOptions.bPunctualLighting)   iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting) iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;
    if(gptData->tRuntimeOptions.bNormalMapping)      iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_NORMAL_MAPS;
    if(gptData->tRuntimeOptions.bPcfShadows)         iSceneWideRenderingFlags |= PL_RENDERING_FLAG_PCF_SHADOWS;
        
    plLightComponent* ptLights = NULL;
    const uint32_t uLightCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);
    int aiLightingConstantData[] = {iSceneWideRenderingFlags, gptData->tRuntimeOptions.tShaderDebugMode, 0};
    ptScene->tDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_directional", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tSpotLightingShader = gptShaderVariant->get_shader("deferred_lighting_spot", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tPointLightingShader = gptShaderVariant->get_shader("deferred_lighting_point", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    int iProbeCount = (int)pl_sb_size(ptScene->sbtProbeData);
    ptScene->tProbeLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, NULL, &iProbeCount, &gptData->tRenderPassLayout);
    aiLightingConstantData[0] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
    aiLightingConstantData[2] = 0;
    ptScene->tEnvDirectionalLightingShader = gptShaderVariant->get_shader("deferred_lighting_directional", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tEnvSpotLightingShader = gptShaderVariant->get_shader("deferred_lighting_spot", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    ptScene->tEnvPointLightingShader = gptShaderVariant->get_shader("deferred_lighting_point", NULL, NULL, aiLightingConstantData, &gptData->tRenderPassLayout);
    gptShader->set_options(&tOriginalOptions);

    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
    for (uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
    {
        plObjectComponent*           ptObject    = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[uDrawableIndex].tEntity);
        plMeshComponent*             ptMesh      = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);
        plMaterialComponent*         ptMaterial  = gptECS->get_component(ptScene->ptComponentLibrary, tMaterialComponentType, ptMesh->tMaterial);
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtDrawables[uDrawableIndex].tEntity);
        
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
        int aiForwardFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->tFlags,
            iObjectRenderingFlags,
            pl_sb_size(ptScene->sbtLightData),
            pl_sb_size(ptScene->sbtProbeData),
            gptData->tRuntimeOptions.tShaderDebugMode
        };

        int aiGBufferFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->tFlags,
            gptData->tRuntimeOptions.tShaderDebugMode,
            iObjectRenderingFlags
        };

        int aiVertexConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride
        };

        if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_DEFERRED)
        {
            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER,
                .ulCullMode           = PL_CULL_MODE_CULL_BACK,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[uDrawableIndex].tShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tRenderPassLayout);
            ptScene->sbtDrawables[uDrawableIndex].tEnvShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);
        }
        else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_FORWARD)
        {

            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[uDrawableIndex].tShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tRenderPassLayout);
            aiForwardFragmentConstantData0[3] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
            ptScene->sbtDrawables[uDrawableIndex].tEnvShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tRenderPassLayout);
        }

        else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
        {

            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[uDrawableIndex].tShader = gptShaderVariant->get_shader("transmission", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
            aiForwardFragmentConstantData0[3] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
            ptScene->sbtDrawables[uDrawableIndex].tEnvShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tRenderPassLayout);
        }

        if(ptMaterial->tAlphaMode != PL_MATERIAL_ALPHA_MODE_OPAQUE)
        {
            plGraphicsState tShadowVariant = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulWireframe          = 0,
                .ulDepthClampEnabled  = 1,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            };
            ptScene->sbtDrawables[uDrawableIndex].tShadowShader = gptShaderVariant->get_shader("alphashadow", &tShadowVariant, aiVertexConstantData0, &aiForwardFragmentConstantData0[1], &gptData->tDepthRenderPassLayout);
        }

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_CAST_SHADOW && ptObject->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW)
        {
            if(ptMaterial->tAlphaMode != PL_MATERIAL_ALPHA_MODE_OPAQUE)
            {
                pl_sb_push(ptScene->sbuShadowForwardDrawables, uDrawableIndex);
            }
            else
            {
                pl_sb_push(ptScene->sbuShadowDeferredDrawables, uDrawableIndex);
            }
        }

        if(ptMesh->tSkinComponent.uIndex == UINT32_MAX)
            continue;

        // stride within storage buffer
        uint32_t uStride = 0;

        uint64_t ulVertexStreamMask = 0;

        // calculate vertex stream mask based on provided data
        if(ptMesh->ptVertexPositions)  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_POSITION; }
        if(ptMesh->ptVertexNormals)    { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
        if(ptMesh->ptVertexTangents)   { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
        if(ptMesh->ptVertexWeights[0]) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
        if(ptMesh->ptVertexWeights[1]) { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
        if(ptMesh->ptVertexJoints[0])  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
        if(ptMesh->ptVertexJoints[1])  { uStride += 1; ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }

        // stride within storage buffer
        uint32_t uDestStride = 0;

        // calculate vertex stream mask based on provided data
        if(ptMesh->ptVertexNormals)               { uDestStride += 1; }
        if(ptMesh->ptVertexTangents)              { uDestStride += 1; }
        if(ptMesh->ptVertexColors[0])             { uDestStride += 1; }
        if(ptMesh->ptVertexColors[1])             { uDestStride += 1; }
        if(ptMesh->ptVertexTextureCoordinates[0]) { uDestStride += 1; }

        int aiSpecializationData[] = {(int)ulVertexStreamMask, (int)uStride, (int)ptMesh->ulVertexStreamMask, (int)uDestStride};
        ptScene->sbtSkinData[ptScene->sbtDrawables[uDrawableIndex].uSkinIndex].tShader = gptShaderVariant->get_compute_shader("skinning", aiSpecializationData); 
    }


    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_add_probe_to_scene(plScene* ptScene, plEntity tProbe)
{
    pl__renderer_create_probe_data(ptScene, tProbe);
    ptScene->bProbeCountDirty = true;
    pl_renderer_add_drawable_objects_to_scene(ptScene, 1, &tProbe);
}

void
pl_renderer_add_light_to_scene(plScene* ptScene, plEntity tEntity)
{
    plRendererLight tRendererLight = {
        .tEntity = tEntity
    };

    plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, tEntity);

    if(ptLight->tType == PL_LIGHT_TYPE_POINT)
    {  

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
            ptScene->uShadowIndex++;

        const plGpuLight tLight = {
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

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
        {
            tRendererLight.uShadowBufferOffset = ptScene->uShadowOffset;
            ptScene->uShadowOffset += sizeof(plMat4) * 6;
        }
    }
    else if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
    {  

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
            ptScene->uShadowIndex++;

        const plGpuLight tLight = {
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

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
        {
            tRendererLight.uShadowBufferOffset = ptScene->uShadowOffset;
            ptScene->uShadowOffset += sizeof(plMat4);
        }
    }
    else if(ptLight->tType == PL_LIGHT_TYPE_DIRECTIONAL)
    {   

        if(ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
            ptScene->uDShadowIndex++;

        const plGpuLight tLight = {
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


    pl_sb_push(ptScene->sbtLights, tRendererLight);
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

    // if transmission is required, ensure we have the backing textures needed
    // for the calculations
    if(ptScene->bTransmissionRequired)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptScene->sbptViews); i++)
        {
            plView* ptView = ptScene->sbptViews[i];

            if(!gptGfx->is_texture_valid(ptDevice, ptView->tTransmissionTexture))
            {
                pl_log_info(gptLog, gptData->uLogChannel, "creating required transmission texture");
                const plTextureDesc tRawOutput2TextureDesc = {
                    .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
                    .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
                    .uLayers       = 1,
                    .uMips         = 0,
                    .tType         = PL_TEXTURE_TYPE_2D,
                    .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT | PL_TEXTURE_USAGE_STORAGE,
                    .pcDebugName   = "offscreen transmission"
                };
                ptView->tTransmissionTexture                = pl__renderer_create_texture(&tRawOutput2TextureDesc,  "transmission offscreen", 0, PL_TEXTURE_USAGE_SAMPLED);
                ptView->tData.iTransmissionFrameBufferIndex = pl__renderer_get_bindless_texture_index(ptScene, ptView->tTransmissionTexture);
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
                pl_log_info(gptLog, gptData->uLogChannel, "freeing unneeded transmission texture");
                gptGfx->queue_texture_for_deletion(ptDevice, ptView->tTransmissionTexture);
            }
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~transform & instance buffer update~~~~~~~~~~~~~~~~~~~~~

    // update transform & instance buffers since we are now using indices
    plBuffer* ptTransformBuffer = gptGfx->get_buffer(ptDevice, ptScene->atTransformBuffer[uFrameIdx]);
    plBuffer* ptInstanceBuffer  = gptGfx->get_buffer(ptDevice, ptScene->atInstanceBuffer[uFrameIdx]);

    uint32_t uInstanceOffset = 0;
    const uint32_t uObjectCount = pl_sb_size(ptScene->sbtDrawables);
    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    if(gptData->tRuntimeOptions.bMultiViewportShadows)
    {
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
    }
    else
    {
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
                            .iViewportIndex  = 0
                        };
                        
                        memcpy(&ptInstanceBuffer->tMemoryAllocation.pHostMapped[uInstanceOffset * sizeof(tShadowInstanceData)], &tShadowInstanceData, sizeof(tShadowInstanceData));
                        uInstanceOffset++;
                    }
                }
            }

        }
    }

    plBuffer* ptSceneBuffer = gptGfx->get_buffer(ptDevice, ptScene->atSceneBuffer[uFrameIdx]);

    if(gptData->tRuntimeOptions.bFog)
    {
        if(gptData->tRuntimeOptions.bLinearFog)
            ptScene->tSceneData.iSceneFlags |= PL_SCENE_FLAG_LINEAR_FOG;
        else
            ptScene->tSceneData.iSceneFlags |= PL_SCENE_FLAG_HEIGHT_FOG;
    }
    else
    {
        ptScene->tSceneData.iSceneFlags &= ~PL_SCENE_FLAG_LINEAR_FOG;
        ptScene->tSceneData.iSceneFlags &= ~PL_SCENE_FLAG_HEIGHT_FOG;
    }

    memcpy(ptSceneBuffer->tMemoryAllocation.pHostMapped, &ptScene->tSceneData, sizeof(plGpuSceneData));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~perform skinning~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tSkinningBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptSkinningCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "skinning");
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

    // plLightComponent* ptLights = NULL;
    // const uint32_t uLightCount = gptECS->get_components(ptScene->ptComponentLibrary, gptData->tLightComponentType, (void**)&ptLights, NULL);

    // TODO: remove this nonsense
    // ptScene->uShadowOffset = 0;
    ptScene->uDShadowOffset = 0;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate shadow maps~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // prep
    pl__renderer_pack_shadow_atlas(ptScene);
    pl_sb_reset(ptScene->sbtLightShadowData);

    const plBeginCommandInfo tShadowBeginInfo = {
        .uWaitSemaphoreCount   = 2,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore(), gptStarter->get_last_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value(), ptScene->uLastSemValueForShadow},
    };

    plCommandBuffer* ptShadowCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "scene shadows");
    gptGfx->begin_command_recording(ptShadowCmdBuffer, &tShadowBeginInfo);

    plRenderEncoder* ptShadowEncoder = gptGfx->begin_render_pass(ptShadowCmdBuffer, ptScene->tFirstShadowRenderPass, NULL);
    gptGfx->push_render_debug_group(ptShadowEncoder, "First Shadow", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

    pl__renderer_generate_shadow_maps(ptShadowEncoder, ptShadowCmdBuffer, ptScene);

    gptGfx->pop_render_debug_group(ptShadowEncoder);
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
    memcpy(ptShadowDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightShadowData, sizeof(plGpuLightShadow) * pl_sb_size(ptScene->sbtLightShadowData));
    
    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);

        if(ptScene->bSheenRequired && !gptGfx->is_texture_valid(ptDevice, ptProbe->tSheenEnvTexture))
        {
            pl_log_info(gptLog, gptData->uLogChannel, "creating required sheen env texture");
            const plTextureDesc tTextureDesc = {
                .tDimensions = {(float)ptProbeComp->uResolution, (float)ptProbeComp->uResolution, 1},
                .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
                .uLayers     = 6,
                .uMips       = (uint32_t)floorf(log2f((float)ptProbeComp->uResolution)) - 3, // guarantee final dispatch during filtering is 16 threads
                .tType       = PL_TEXTURE_TYPE_CUBE,
                .tUsage      = PL_TEXTURE_USAGE_SAMPLED
            };
            ptProbe->tSheenEnvTexture = pl__renderer_create_texture(&tTextureDesc, "sheen texture", 0, PL_TEXTURE_USAGE_SAMPLED);
            ptProbe->uSheenEnvSampler = pl__renderer_get_bindless_cube_texture_index(ptScene, ptProbe->tSheenEnvTexture);
            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
        }
        else if(!ptScene->bSheenRequired && gptGfx->is_texture_valid(ptDevice, ptProbe->tSheenEnvTexture))
        {
            pl_log_info(gptLog, gptData->uLogChannel, "freeing unneeded sheen env texture");
            gptGfx->queue_texture_for_deletion(ptDevice, ptProbe->tSheenEnvTexture);
        }

        
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
                .tPosDouble   = {(double)ptProbeTransform->tTranslation.x, (double)ptProbeTransform->tTranslation.y, (double)ptProbeTransform->tTranslation.z},
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

            plCommandBuffer* ptCSMCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "probe csm");
            gptGfx->begin_command_recording(ptCSMCommandBuffer, &tBeginCSMInfo);

            plRenderEncoder* ptCSMEncoder = gptGfx->begin_render_pass(ptCSMCommandBuffer, ptScene->tShadowRenderPass, NULL);
            gptGfx->push_render_debug_group(ptCSMEncoder, "Probe CSM", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});

            pl__renderer_generate_cascaded_shadow_map(ptCSMEncoder, ptCSMCommandBuffer, ptScene, uFace, uProbeIndex, 1, &ptProbe->tDirectionLightShadowData,  &atEnvironmentCamera[uFace]);

            gptGfx->pop_render_debug_group(ptCSMEncoder);
            gptGfx->end_render_pass(ptCSMEncoder);
            gptGfx->end_command_recording(ptCSMCommandBuffer);

            const plSubmitInfo tSubmitCSMInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
                .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
            };
            gptGfx->submit_command_buffer(ptCSMCommandBuffer, &tSubmitCSMInfo);
            gptGfx->return_command_buffer(ptCSMCommandBuffer);

            const plGpuViewData tProbeBindGroupBuffer = {
                .tViewportSize         = {.x = ptProbe->tTargetSize.x, .y = ptProbe->tTargetSize.y, .z = 1.0f, .w = 1.0f},
                .tCameraPos            = atEnvironmentCamera[uFace].tPos,
                .tCameraProjection     = atEnvironmentCamera[uFace].tProjMat,
                .tCameraView           = atEnvironmentCamera[uFace].tViewMat,
                .tCameraProjectionInv  = pl_mat4_invert(&atEnvironmentCamera[uFace].tProjMat),
                .tCameraViewInv        = pl_mat4_invert(&atEnvironmentCamera[uFace].tViewMat),
                .tCameraViewProjection = pl_mul_mat4(&atEnvironmentCamera[uFace].tProjMat, &atEnvironmentCamera[uFace].tViewMat)
            };

            // copy global buffer data for probe rendering
            const uint32_t uProbeGlobalBufferOffset = sizeof(plGpuViewData) * uFace;
            plBuffer* ptProbeGlobalBuffer = gptGfx->get_buffer(ptDevice, ptProbe->atView2Buffers[uFrameIdx]);
            memcpy(&ptProbeGlobalBuffer->tMemoryAllocation.pHostMapped[uProbeGlobalBufferOffset], &tProbeBindGroupBuffer, sizeof(plGpuViewData));
        }

        // copy probe shadow data to GPU buffer
        plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptProbe->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx]);
        memcpy(ptDShadowDataBuffer->tMemoryAllocation.pHostMapped, ptProbe->tDirectionLightShadowData.sbtDLightShadowData, sizeof(plGpuLightShadow) * pl_sb_size(ptProbe->tDirectionLightShadowData.sbtDLightShadowData));

    }

    // update light GPU side buffers
    const uint32_t uLightCount = pl_sb_size(ptScene->sbtLights);
    for(uint32_t i = 0; i < uLightCount; i++)
    {
        plLightComponent* ptLight = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tLightComponentType, ptScene->sbtLights[i].tEntity);

        ptScene->sbtLightData[i].tPosition     = ptLight->tPosition;
        ptScene->sbtLightData[i].fIntensity    = ptLight->fIntensity;
        ptScene->sbtLightData[i].tDirection    = ptLight->tDirection;
        ptScene->sbtLightData[i].tColor        = ptLight->tColor;
        ptScene->sbtLightData[i].fRange        = ptLight->fRange;
        ptScene->sbtLightData[i].iCascadeCount = (int)ptLight->uCascadeCount;
        ptScene->sbtLightData[i].iCastShadow   = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW;
        ptScene->sbtLightData[i].iType         = ptLight->tType;
        // ptScene->sbtLightData[i].iShadowIndex = 0; // done at load time

        if(ptLight->tType == PL_LIGHT_TYPE_SPOT)
        {
            ptScene->sbtLightData[i].fInnerConeCos = cosf(ptLight->fInnerConeAngle);
            ptScene->sbtLightData[i].fOuterConeCos = cosf(ptLight->fOuterConeAngle);
        }
        
    }
    plBuffer* ptLightingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atLightBuffer[uFrameIdx]);
    memcpy(ptLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightData, sizeof(plGpuLight) * pl_sb_size(ptScene->sbtLightData));

    pl_sb_reset(ptScene->sbtGPUProbeData);
    for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtProbeData); i++)
    {
        plEnvironmentProbeComponent* ptProbe = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtProbeData[i].tEntity);
        plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtProbeData[i].tEntity);
        plTransformComponent* ptProbeTransform = gptECS->get_component(ptScene->ptComponentLibrary, tTransformComponentType, ptScene->sbtProbeData[i].tEntity);
        plGpuProbe tProbeData = {
            .tPosition              = ptProbeTransform->tTranslation,
            .fRangeSqr              = ptProbe->fRange * ptProbe->fRange,
            .uGGXEnvSampler         = ptScene->sbtProbeData[i].uGGXEnvSampler,
            .uLambertianEnvSampler  = ptScene->sbtProbeData[i].uLambertianEnvSampler,
            .uCharlieEnvSampler     = ptScene->sbtProbeData[i].uSheenEnvSampler,
            .tMin.xyz               = ptObject->tAABB.tMin,
            .tMax.xyz               = ptObject->tAABB.tMax,
            .iMips                  = ptScene->sbtProbeData[i].iMips,
            .iParallaxCorrection    = (int)(ptProbe->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX)
        };
        pl_sb_push(ptScene->sbtGPUProbeData, tProbeData);
    }

    if(pl_sb_size(ptScene->sbtGPUProbeData) > 0)
    {

        plBuffer* ptProbeDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->atGPUProbeDataBuffers[uFrameIdx]);
        memcpy(ptProbeDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtGPUProbeData, sizeof(plGpuProbe) * pl_sb_size(ptScene->sbtGPUProbeData));
    }

    // update environment probes
    if(uFrameIdx == 0) // multiple frames in flight may fight
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

    ptView->tData.fFogHeight = gptData->tRuntimeOptions.fFogHeight;
    ptView->tData.fFogCutOffDistance = gptData->tRuntimeOptions.fFogCutOffDistance;
    ptView->tData.fFogMaxOpacity = gptData->tRuntimeOptions.fFogMaxOpacity;
    ptView->tData.fFogStart = gptData->tRuntimeOptions.fFogStart;
    ptView->tData.fFogHeightFalloff = pl_maxf(0.0f, gptData->tRuntimeOptions.fFogHeightFalloff);
    ptView->tData.tFogColor = gptData->tRuntimeOptions.tFogColor;
    ptView->tData.fFogLinearParam0 = 1.0f / (ptView->tData.fFogCutOffDistance - ptView->tData.fFogStart);
    ptView->tData.fFogLinearParam1 = -ptView->tData.fFogStart / (ptView->tData.fFogCutOffDistance - ptView->tData.fFogStart);
    
    const float fFogDensity = -(float)(ptView->tData.fFogHeightFalloff * (ptCamera->tPos.y - ptView->tData.fFogHeight));
    ptView->tData.tFogDensity = (plVec3){gptData->tRuntimeOptions.fFogDensity, fFogDensity, gptData->tRuntimeOptions.fFogDensity * expf(fFogDensity)};

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate CSMs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tCSMBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptCSMCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "view csm");
    gptGfx->begin_command_recording(ptCSMCmdBuffer, &tCSMBeginInfo);

    pl_sb_reset(ptView->tDirectionLightShadowData.sbtDLightShadowData);
    ptView->tDirectionLightShadowData.uOffset = 0;
    ptView->tDirectionLightShadowData.uOffsetIndex = 0;

    plRenderEncoder* ptCSMEncoder = gptGfx->begin_render_pass(ptCSMCmdBuffer, ptScene->tShadowRenderPass, NULL);
    
    gptGfx->push_render_debug_group(ptCSMEncoder, "View CSM", (plVec4){0.33f, 0.02f, 0.10f, 1.0f});
    pl__renderer_generate_cascaded_shadow_map(ptCSMEncoder, ptCSMCmdBuffer, ptScene, ptView->uIndex, 0, 0, &ptView->tDirectionLightShadowData,  ptCamera);

    gptGfx->pop_render_debug_group(ptCSMEncoder);
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
    memcpy(ptDShadowDataBuffer->tMemoryAllocation.pHostMapped, ptView->tDirectionLightShadowData.sbtDLightShadowData, sizeof(plGpuLightShadow) * pl_sb_size(ptView->tDirectionLightShadowData.sbtDLightShadowData));

    if(gptData->tRuntimeOptions.bShowProbes)
    {
        const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
        for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
        {
            plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
            plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptProbe->tEntity);
            plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptProbe->tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptObject->tTransform);
            gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.0f, 1.0f, 0.0f), .fThickness = 0.02f});
            plSphere tSphere = {
                .fRadius = ptProbeComp->fRange,
                .tCenter = ptTransform->tTranslation
            };
            gptDraw->add_3d_sphere(ptView->pt3DDrawList, tSphere, 6, 6, (plDrawLineOptions){.uColor = PL_COLOR_32_LIGHT_GREY, .fThickness = 0.005f});
        }
    }

    plBuffer* ptViewBuffer = gptGfx->get_buffer(ptDevice, ptView->atViewBuffers[uFrameIdx]);
    memcpy(ptViewBuffer->tMemoryAllocation.pHostMapped, &ptView->tData, sizeof(plGpuViewData));
    uint32_t uBloomTextureCount = pl_sb_size(ptView->sbtBloomDownChain);

    if(gptData->tRuntimeOptions.bBloomActive && uBloomTextureCount < gptData->tRuntimeOptions.uBloomChainLength)
    {

        uint32_t uNewTexturesNeeded = gptData->tRuntimeOptions.uBloomChainLength - uBloomTextureCount;

        plTextureDesc tBloomTextureDesc = {
            .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
            .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_STORAGE,
            .pcDebugName   = "bloom"
        };
    
        tBloomTextureDesc.tDimensions.x *= powf(0.5f, (float)uBloomTextureCount);
        tBloomTextureDesc.tDimensions.y *= powf(0.5f, (float)uBloomTextureCount);

        for(uint32_t i = 0; i < uNewTexturesNeeded; i++)
        {

            if(tBloomTextureDesc.tDimensions.x < 1.0f || tBloomTextureDesc.tDimensions.y < 1.0f)
            {
                gptData->tRuntimeOptions.uBloomChainLength -= uNewTexturesNeeded - i;
                break;
            }

            plTextureHandle tBloomUpTexture = pl__renderer_create_texture(&tBloomTextureDesc,  "bloom", i, PL_TEXTURE_USAGE_SAMPLED);
            plTextureHandle tBloomDownTexture = pl__renderer_create_texture(&tBloomTextureDesc,  "bloom", i, PL_TEXTURE_USAGE_SAMPLED);

            pl_sb_push(ptView->sbtBloomDownChain, tBloomDownTexture);
            pl_sb_push(ptView->sbtBloomUpChain, tBloomUpTexture);

            tBloomTextureDesc.tDimensions.x *= 0.5f;
            tBloomTextureDesc.tDimensions.y *= 0.5f;
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
    plScene*       ptScene   = ptView->ptParentScene;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~culling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_cpu_sample(gptProfile, 0, "culling");
    
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
    pl_end_cpu_sample(gptProfile, 0); // culling

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plGpuViewData tBindGroupBuffer = {
        .tViewportSize         = {.xy = ptView->tTargetSize, .ignored0_ = 1.0f, .ignored1_ = 1.0f},
        .tCameraPos            = ptCamera->tPos,
        .tCameraProjection     = ptCamera->tProjMat,
        .tCameraProjectionInv  = pl_mat4_invert(&ptCamera->tProjMat),
        .tCameraViewInv        = pl_mat4_invert(&ptCamera->tViewMat),
        .tCameraView           = ptCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat)
    };
    memcpy(gptGfx->get_buffer(ptDevice, ptView->atView2Buffers[uFrameIdx])->tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(plGpuViewData));
    
    const plBindGroupUpdateBufferData atViewBGBufferData[] = 
    {
        { .uSlot = 0, .tBuffer = ptView->atViewBuffers[uFrameIdx], .szBufferRange = sizeof(plGpuViewData) },
        { .uSlot = 1, .tBuffer = ptView->atView2Buffers[uFrameIdx], .szBufferRange = sizeof(plGpuViewData) },
        { .uSlot = 2, .tBuffer = ptScene->atLightBuffer[uFrameIdx], .szBufferRange = sizeof(plGpuLight) * pl_sb_size(ptScene->sbtLightData)},
        { .uSlot = 3, .tBuffer = ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGpuLightShadow) * pl_sb_size(ptView->tDirectionLightShadowData.sbtDLightShadowData)},
        { .uSlot = 4, .tBuffer = ptScene->atLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGpuLightShadow) * pl_sb_size(ptScene->sbtLightShadowData)},
        { .uSlot = 5, .tBuffer = ptScene->atGPUProbeDataBuffers[uFrameIdx], .szBufferRange = sizeof(plGpuProbe) * pl_sb_size(ptScene->sbtGPUProbeData)},
    };

    const plBindGroupUpdateData tViewBGData = {
        .uBufferCount      = 6,
        .atBufferBindings  = atViewBGBufferData,
    };

    const plBindGroupDesc tViewBGDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptData->tViewBGLayout,
        .pcDebugName = "light bind group 2"
    };

    plBindGroupHandle tViewBG = gptGfx->create_bind_group(ptDevice, &tViewBGDesc);
    gptGfx->update_bind_group(gptData->ptDevice, tViewBG, &tViewBGData);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tViewBG);

    gptJob->wait_for_counter(ptCullCounter);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~binning based on pass type~~~~~~~~~~~~~~~~~~~~~~~~~
    
    pl_begin_cpu_sample(gptProfile, 0, "binning");

    if(ptCullCamera)
    {
        pl_sb_reset(ptView->sbuVisibleDeferredEntities);
        pl_sb_reset(ptView->sbuVisibleForwardEntities);
        pl_sb_reset(ptView->sbuVisibleTransmissionEntities);
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
                else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
                {
                    pl_sb_push(ptView->sbuVisibleTransmissionEntities, uDrawableIndex);
                    pl_sb_push(ptView->sbtVisibleDrawables, uDrawableIndex);
                }
                
            }
        }
    }
    pl_end_cpu_sample(gptProfile, 0); // binning

    const plBeginCommandInfo tSceneBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarter->get_current_timeline_semaphore()},
        .auWaitSemaphoreValues = {gptStarter->get_current_timeline_value()},
    };

    plCommandBuffer* ptSceneCmdBuffer = gptGfx->request_command_buffer(ptCmdPool, "main scene");
    gptGfx->begin_command_recording(ptSceneCmdBuffer, &tSceneBeginInfo);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~main render pass work~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plRenderEncoder* ptMainEncoder = gptGfx->begin_render_pass(ptSceneCmdBuffer, ptView->tRenderPass, NULL);

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
        .sbuVisibleDeferredEntities = ptView->sbuVisibleDeferredEntities,
        .uGlobalIndex = 0,
        .tBG2 = ptView->atDeferredBG1[uFrameIdx],
        .ptArea = &tArea
    };
    pl__render_view_gbuffer_fill_pass(ptScene, ptMainEncoder, &tGbufferFillPassInfo);

    gptGfx->next_subpass(ptMainEncoder, NULL);

    plDeferredLightingPassInfo tDeferredLightingPassInfo = {
        .tBG2 = ptView->tLightingBindGroup,
        .ptArea = &tArea
    };

    if(gptData->tRuntimeOptions.tShaderDebugMode == PL_SHADER_DEBUG_MODE_NONE)
        pl__render_view_deferred_lighting_pass(ptScene, ptMainEncoder, tViewBG, &tDeferredLightingPassInfo);
    else
        pl__render_view_deferred_lighting_debug_pass(ptView, ptMainEncoder, tViewBG);

    gptGfx->next_subpass(ptMainEncoder, NULL);

    gptGfx->push_render_debug_group(ptMainEncoder, "Forward Lighting", (plVec4){0.33f, 0.02f, 0.20f, 1.0f});

    if(ptScene->tSkyboxTexture.uIndex != 0 && ptView->bShowSkybox)
    {
        ptView->bShowSkybox = false;
        plMat4 tTransformMat = pl_mat4_translate_vec3(ptCamera->tPos);
        pl__render_view_skybox_pass(ptScene, ptMainEncoder, tViewBG, &tTransformMat, &tArea, 0);
    }
    
    plForwardPassInfo tForwardPassInfo = {
        .sbuVisibleEntities = ptView->sbuVisibleForwardEntities,
        .ptArea = &tArea,
        .uGlobalIndex = 0,
        .bProbe = false
    };
    pl__render_view_forward_pass(ptScene, ptMainEncoder, tViewBG, &tForwardPassInfo);

    gptGfx->pop_render_debug_group(ptMainEncoder);

    // end main render pass
    gptGfx->end_render_pass(ptMainEncoder);

    // if transmission is required, we will blit whole screen to
    // offscreen texture used for transmission calculations.
    // The separation of the command buffers is just for sync
    // purposes & should probably be reworked soon.
    if(ptView->ptParentScene->bTransmissionRequired)
    {
        gptGfx->end_command_recording(ptSceneCmdBuffer);

        const plSubmitInfo tScenePreSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarter->get_current_timeline_semaphore()},
            .auSignalSemaphoreValues = {gptStarter->increment_current_timeline_value()}
        };
        gptGfx->submit_command_buffer(ptSceneCmdBuffer, &tScenePreSubmitInfo);
        gptGfx->return_command_buffer(ptSceneCmdBuffer);

        ptSceneCmdBuffer = pl__render_view_full_screen_blit(ptView, &tSceneBeginInfo);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~secondary render pass work~~~~~~~~~~~~~~~~~~~~~~~~~~

    plRenderEncoder* ptTransparentEncoder = gptGfx->begin_render_pass(ptSceneCmdBuffer, ptView->tTransparentRenderPass, NULL);
    gptGfx->set_depth_bias(ptTransparentEncoder, 0.0f, 0.0f, 0.0f);
    
    if(pl_sb_size(ptView->sbuVisibleTransmissionEntities) > 0)
        pl__render_view_transmission_pass(ptView, ptTransparentEncoder, tViewBG);

    if(ptView->bShowGrid)
        pl__render_view_grid_pass(ptView, ptTransparentEncoder, ptCamera);

    gptGfx->end_render_pass(ptTransparentEncoder);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity selection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptView->bRequestHoverCheck)
        pl__render_view_pick_pass(ptView, tViewBG, ptSceneCmdBuffer);

    // finished core rendering work (mostly separating command buffers for sync purposes)
    gptGfx->end_command_recording(ptSceneCmdBuffer);

    const plSubmitInfo tSceneSubmitInfo = {
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

    if(gptData->tRuntimeOptions.bBloomActive && pl_sb_size(ptView->sbtBloomDownChain) >= gptData->tRuntimeOptions.uBloomChainLength)
        pl__render_view_bloom_pass(ptView);

    pl__render_view_tonemap_pass(ptView);

    // add debug stuff after post processing so things like bloom & color don't get
    // affected
    pl__render_view_debug_pass(ptView, ptCamera, ptCullCamera);

    // update stats
    static double* pdVisibleOpaqueObjects = NULL;
    static double* pdVisibleTransparentObjects = NULL;

    // recording draw call stats
    *gptData->pdDrawCalls += (double)(pl_sb_size(ptView->sbuVisibleDeferredEntities) + pl_sb_size(ptView->sbuVisibleForwardEntities) + pl_sb_size(ptView->sbuVisibleTransmissionEntities) + 1);
    
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

    gptStage->new_frame();
    gptGfx->reset_bind_group_pool(gptData->aptTempGroupPools[gptGfx->get_current_frame_index()]);
    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);


    // perform GPU buffer updates
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarter->get_current_command_pool();
    plTimelineSemaphore* tSemHandle = gptStarter->get_current_timeline_semaphore();

    for(uint32_t uSceneIndex = 0; uSceneIndex < pl_sb_size(gptData->sbptScenes); uSceneIndex++)
    {
        plScene* ptScene = gptData->sbptScenes[uSceneIndex];
        if(!ptScene->bActive)
            continue;

        if(ptScene->bProbeCountDirty)
        {
            ptScene->bProbeCountDirty = false;
            int iProbeCount = (int)pl_sb_size(ptScene->sbtProbeData);
            ptScene->tProbeLightingShader = gptShaderVariant->get_shader("deferred_lighting", NULL, NULL, &iProbeCount, &gptData->tRenderPassLayout);
        }

        if(ptScene->uMaterialDirtyValue > 0)
        {
            if(gptStage->completed(ptScene->uMaterialDirtyValue))
            {
                const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();

                const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
                for(uint32_t i = 0; i < uDrawableCount; i++)
                {
                    plObjectComponent* ptObject = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);
                    plMeshComponent* ptMesh = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);

                    uint32_t uMaterialIndex = (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, ptMesh->tMaterial.uData);
                    ptScene->sbtDrawables[i].uMaterialIndex = (uint32_t)ptScene->sbtMaterialNodes[uMaterialIndex]->uOffset / sizeof(plGpuMaterial);
                }
                ptScene->uMaterialDirtyValue = 0;
            }
        }

        if(ptScene->bObjectCountDirty)
        {
            ptScene->bObjectCountDirty = false;
            const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtDrawables);
            const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();

            // sort drawables by mesh to prepare for instancing (consecutive drawables with same mesh will be instanced together)
            for (uint32_t i = 0; i < uDrawableCount; i++)
            {
                ptScene->sbtDrawables[i].uInstanceCount = 1;
                ptScene->sbtDrawables[i].uTransformIndex = ptScene->uNextTransformIndex++;
                plObjectComponent* ptObjectA = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[i].tEntity);
                for (uint32_t j = i; j < uDrawableCount - 1; j++)
                {
                    plObjectComponent* ptObjectB = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[j + 1].tEntity);
                    if(ptObjectA->tMesh.uIndex == ptObjectB->tMesh.uIndex)
                    {
                        // increment instance count of first drawable
                        ptScene->sbtDrawables[i].uInstanceCount++;

                        // set instance count of duplicate drawable to 0 so it won't be rendered separately
                        ptScene->sbtDrawables[j + 1].uInstanceCount = 0;
                        ptScene->sbtDrawables[j + 1].uTransformIndex = ptScene->uNextTransformIndex++;
                    }
                    else // found end of duplicates
                        break;
                    
                }
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

bool
pl_renderer_add_drawable_objects_to_scene(plScene* ptScene, uint32_t uObjectCount, const plEntity* atObjects)
{

    if(uObjectCount == 0)
        return true;

    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    int iSceneWideRenderingFlags = 0;
    if(gptData->tRuntimeOptions.bPunctualLighting)   iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting) iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;
    if(gptData->tRuntimeOptions.bNormalMapping)      iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_NORMAL_MAPS;
    if(gptData->tRuntimeOptions.bPcfShadows)         iSceneWideRenderingFlags |= PL_RENDERING_FLAG_PCF_SHADOWS;
        
    ptScene->bObjectCountDirty = true;

    uint32_t uStart = pl_sb_size(ptScene->sbtDrawables);
    pl_sb_add_n(ptScene->sbtDrawables, uObjectCount);
    pl_sb_add_n(ptScene->sbtDrawableResources, uObjectCount);

    const plEcsTypeKey tMeshComponentType = gptMesh->get_ecs_type_key_mesh();
    const plEcsTypeKey tMaterialComponentType = gptMaterial->get_ecs_type_key();

    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        ptScene->sbtDrawables[uStart + i].tEntity = atObjects[i];
        ptScene->sbtDrawables[uStart + i].uSkinIndex = UINT32_MAX;
    }

    // sort to group entities for instances (slow bubble sort, improve later)
    bool bSwapped = false;
    for (uint32_t i = 0; i < uObjectCount - 1; i++)
    {
        bSwapped = false;
        for (uint32_t j = 0; j < uObjectCount - i - 1; j++)
        {
            plObjectComponent* ptObjectA = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[uStart + j].tEntity);
            plObjectComponent* ptObjectB = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[uStart + j + 1].tEntity);
            if (ptObjectA->tMesh.uIndex > ptObjectB->tMesh.uIndex)
            {
                plEntity tA = ptScene->sbtDrawables[uStart + j].tEntity;
                plEntity tB = ptScene->sbtDrawables[uStart + j + 1].tEntity;
                ptScene->sbtDrawables[uStart + j].tEntity = tB;
                ptScene->sbtDrawables[uStart + j + 1].tEntity = tA;
                bSwapped = true;
            }
        }
      
        // if no two elements were swapped, then break
        if (!bSwapped)
            break;
    }

    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        uint32_t uDrawableIndex = uStart + i;
        pl_hm_insert(&ptScene->tDrawableHashmap, ptScene->sbtDrawables[uDrawableIndex].tEntity.uData, uDrawableIndex);
        bool bResult = pl__renderer_add_drawable_data_to_global_buffer(ptScene, uDrawableIndex);
        if(!bResult)
        {
            pl_sb_del_n(ptScene->sbtDrawables, uStart, uObjectCount);
            pl_sb_del_n(ptScene->sbtDrawableResources, uStart, uObjectCount);
            pl_end_cpu_sample(gptProfile, 0);
            return false;
        }

        plObjectComponent*           ptObject    = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tObjectComponentType, ptScene->sbtDrawables[uDrawableIndex].tEntity);
        plMeshComponent*             ptMesh      = gptECS->get_component(ptScene->ptComponentLibrary, tMeshComponentType, ptObject->tMesh);
        plMaterialComponent*         ptMaterial  = gptECS->get_component(ptScene->ptComponentLibrary, tMaterialComponentType, ptMesh->tMaterial);
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(ptScene->ptComponentLibrary, gptData->tEnvironmentProbeComponentType, ptScene->sbtDrawables[uDrawableIndex].tEntity);
        
        if(ptProbeComp)
            ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_PROBE | PL_DRAWABLE_FLAG_FORWARD;
        else // regular object
        {
            bool bForward = false;

            // use forward renderer if material is transparent
            if(ptMaterial->tAlphaMode == PL_MATERIAL_ALPHA_MODE_BLEND)
                bForward = true;

            // use forward renderer if material is advanced (can't fit required properties in gbuffer)
            if(ptMaterial->tShaderType == PL_SHADER_TYPE_PBR_ADVANCED)
                bForward = true;

            // check if transmission textures are required
            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_TRANSMISSION || ptMaterial->tFlags & PL_MATERIAL_FLAG_VOLUME || ptMaterial->tFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
            {
                ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_TRANSMISSION;
                ptScene->bTransmissionRequired = true;
            }
            else if(bForward)
            {
                if(ptMaterial->tFlags & PL_MATERIAL_FLAG_SHEEN) // check if sheen is required (additional scene textures)
                    ptScene->bSheenRequired = true;
                ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_FORWARD;
            }
            else
                ptScene->sbtDrawables[uDrawableIndex].tFlags = PL_DRAWABLE_FLAG_DEFERRED;
        }

        uint64_t uMaterialIndex = pl_renderer__add_material_to_scene(ptScene, ptMesh->tMaterial);

        ptScene->sbtDrawables[uDrawableIndex].uMaterialIndex = (uint32_t)ptScene->sbtMaterialNodes[uMaterialIndex]->uOffset;

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
        int aiForwardFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->tFlags,
            iObjectRenderingFlags,
            pl_sb_size(ptScene->sbtLightData),
            pl_sb_size(ptScene->sbtProbeData),
            gptData->tRuntimeOptions.tShaderDebugMode
        };

        int aiGBufferFragmentConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iTextureMappingFlags,
            ptMaterial->tFlags,
            gptData->tRuntimeOptions.tShaderDebugMode,
            iObjectRenderingFlags
        };

        int aiVertexConstantData0[] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride
        };

        if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_DEFERRED)
        {

            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER,
                .ulCullMode           = PL_CULL_MODE_CULL_BACK,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[uDrawableIndex].tShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, &gptData->tRenderPassLayout);
            ptScene->sbtDrawables[uDrawableIndex].tEnvShader = gptShaderVariant->get_shader("gbuffer_fill", &tVariantTemp, aiVertexConstantData0, aiGBufferFragmentConstantData0, NULL);
        }

        else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_FORWARD)
        {

            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[uDrawableIndex].tShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tRenderPassLayout);
            aiForwardFragmentConstantData0[3] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
            ptScene->sbtDrawables[uDrawableIndex].tEnvShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tRenderPassLayout);
        }

        else if(ptScene->sbtDrawables[uDrawableIndex].tFlags & PL_DRAWABLE_FLAG_TRANSMISSION)
        {

            plGraphicsState tVariantTemp = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
                .ulWireframe          = gptData->tRuntimeOptions.bWireframe
            };

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            ptScene->sbtDrawables[uDrawableIndex].tShader = gptShaderVariant->get_shader("transmission", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tTransparentRenderPassLayout);
            aiForwardFragmentConstantData0[3] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
            ptScene->sbtDrawables[uDrawableIndex].tEnvShader = gptShaderVariant->get_shader("forward", &tVariantTemp, aiVertexConstantData0, aiForwardFragmentConstantData0, &gptData->tRenderPassLayout);
        }

        if(ptMaterial->tAlphaMode != PL_MATERIAL_ALPHA_MODE_OPAQUE)
        {
            plGraphicsState tShadowVariant = {
                .ulDepthWriteEnabled  = 1,
                .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulWireframe          = 0,
                .ulDepthClampEnabled  = 1,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            };
            ptScene->sbtDrawables[uDrawableIndex].tShadowShader = gptShaderVariant->get_shader("alphashadow", &tShadowVariant, aiVertexConstantData0, &aiForwardFragmentConstantData0[1], &gptData->tDepthRenderPassLayout);
        }

        if(ptMaterial->tFlags & PL_MATERIAL_FLAG_CAST_SHADOW && ptObject->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW)
        {
            if(ptMaterial->tAlphaMode != PL_MATERIAL_ALPHA_MODE_OPAQUE)
            {
                pl_sb_push(ptScene->sbuShadowForwardDrawables, uDrawableIndex);
            }
            else
            {
                pl_sb_push(ptScene->sbuShadowDeferredDrawables, uDrawableIndex);
            }
        }

        ptScene->sbtDrawables[uDrawableIndex].tIndexBuffer = ptScene->sbtDrawables[uDrawableIndex].uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer;
    }

    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

void
pl_renderer_update_scene_material(plScene* ptScene, plEntity tMaterialComp)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    const plEcsTypeKey tMaterialComponentType = gptMaterial->get_ecs_type_key();

    plMaterialComponent* ptMaterial = gptECS->get_component(ptScene->ptComponentLibrary, tMaterialComponentType, tMaterialComp);

    uint32_t uMaterialIndex = (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, tMaterialComp.uData);
    uint32_t uOldMaterialIndex = 0;
    uint32_t uNewMaterialIndex = 0;
    plFreeListNode* ptOldNode = NULL;
    plFreeListNode* ptNewNode = NULL;
    if(uMaterialIndex == UINT32_MAX)
    {
        PL_ASSERT(false && "material isn't in scene");
        pl_end_cpu_sample(gptProfile, 0);
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
        tGPUMaterial.tEmissiveFactor           = ptMaterial->tEmissiveColor.rgb;
        tGPUMaterial.fAlphaCutoff              = ptMaterial->fAlphaCutoff;
        tGPUMaterial.fClearcoatFactor          = ptMaterial->fClearcoat;
        tGPUMaterial.fClearcoatRoughnessFactor = ptMaterial->fClearcoatRoughness;
        tGPUMaterial.fSheenRoughnessFactor     = ptMaterial->fSheenRoughness;
        tGPUMaterial.fNormalMapStrength        = ptMaterial->fNormalMapStrength;
        tGPUMaterial.fEmissiveStrength         = ptMaterial->fEmissiveStrength;
        tGPUMaterial.tSheenColorFactor         = ptMaterial->tSheenColor;
        tGPUMaterial.fIridescenceFactor        = ptMaterial->fIridescenceFactor;
        tGPUMaterial.fIridescenceIor           = ptMaterial->fIridescenceIor;
        tGPUMaterial.fIridescenceThicknessMin  = ptMaterial->fIridescenceThicknessMin;
        tGPUMaterial.fIridescenceThicknessMax  = ptMaterial->fIridescenceThicknessMax;
        tGPUMaterial.tAnisotropy.x             = cosf(ptMaterial->fAnisotropyRotation);
        tGPUMaterial.tAnisotropy.y             = sinf(ptMaterial->fAnisotropyRotation);
        tGPUMaterial.tAnisotropy.z             = ptMaterial->fAnisotropyStrength;
        tGPUMaterial.fOcclusionStrength        = ptMaterial->fOcclusionStrength;
        tGPUMaterial.tAlphaMode                = ptMaterial->tAlphaMode;
        tGPUMaterial.fTransmissionFactor       = ptMaterial->fTransmissionFactor;
        tGPUMaterial.fThickness                = ptMaterial->fThickness;
        tGPUMaterial.fAttenuationDistance      = ptMaterial->fAttenuationDistance;
        tGPUMaterial.tAttenuationColor         = ptMaterial->tAttenuationColor;
        tGPUMaterial.fDispersion               = ptMaterial->fDispersion;
        tGPUMaterial.fIor                      = ptMaterial->fIor;
        tGPUMaterial.fDiffuseTransmission      = ptMaterial->fDiffuseTransmission;
        tGPUMaterial.tDiffuseTransmissionColor = ptMaterial->tDiffuseTransmissionColor;

        const int iDummyIndex = (int)pl__renderer_get_bindless_texture_index(ptScene, gptData->tDummyTexture);
        for(uint32_t uTextureIndex = 0; uTextureIndex < PL_TEXTURE_SLOT_COUNT; uTextureIndex++)
        {
            tGPUMaterial.aiTextureUVSet[uTextureIndex] = (int)ptMaterial->atTextureMaps[uTextureIndex].uUVSet;
            tGPUMaterial.atTextureTransforms[uTextureIndex] = ptMaterial->atTextureMaps[uTextureIndex].tTransform;
            if(gptResource->is_valid(ptMaterial->atTextureMaps[uTextureIndex].tResource))
            {
                plTextureHandle tValidTexture = gptResource->get_texture(ptMaterial->atTextureMaps[uTextureIndex].tResource);
                tGPUMaterial.aiTextureIndices[uTextureIndex] = (int)pl__renderer_get_bindless_texture_index(ptScene, tValidTexture);
            }
            else
                tGPUMaterial.aiTextureIndices[uTextureIndex] = iDummyIndex;
        }

        ptScene->uMaterialDirtyValue = gptStage->queue_buffer_upload(ptScene->tMaterialDataBuffer,
            ptNewNode->uOffset,
            &tGPUMaterial,
            sizeof(plGpuMaterial));
    }



    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_renderer_add_materials_to_scene(plScene* ptScene, uint32_t uMaterialCount, const plEntity* atMaterials)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    const plEcsTypeKey tMaterialComponentType = gptMaterial->get_ecs_type_key();

    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        const plEntity tMaterialComp = atMaterials[i];
        pl_renderer__add_material_to_scene(ptScene, tMaterialComp);
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

void
pl_renderer_show_grid(plView* ptView)
{
    ptView->bShowGrid = true;
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

    const plComponentDesc tSkinDesc = {
        .pcName  = "Skin",
        .szSize  = sizeof(plSkinComponent),
        .cleanup = pl__renderer_skin_cleanup,
        .reset   = pl__renderer_skin_cleanup,
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

    // not used yet

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
    tApi.initialize                          = pl_renderer_initialize;
    tApi.cleanup                             = pl_renderer_cleanup;
    tApi.create_scene                        = pl_renderer_create_scene;
    tApi.cleanup_scene                       = pl_renderer_cleanup_scene;
    tApi.add_drawable_objects_to_scene       = pl_renderer_add_drawable_objects_to_scene;
    tApi.add_probe_to_scene                  = pl_renderer_add_probe_to_scene;
    tApi.add_light_to_scene                  = pl_renderer_add_light_to_scene;
    tApi.add_materials_to_scene              = pl_renderer_add_materials_to_scene;
    tApi.update_scene_material               = pl_renderer_update_scene_material;
    tApi.create_view                         = pl_renderer_create_view;
    tApi.cleanup_view                        = pl_renderer_cleanup_view;
    tApi.begin_frame                         = pl_renderer_begin_frame;
    tApi.load_skybox_from_panorama           = pl_renderer_load_skybox_from_panorama;
    tApi.reload_scene_shaders                = pl_renderer_reload_scene_shaders;
    tApi.outline_entities                    = pl_renderer_outline_entities;
    tApi.prepare_scene                       = pl_renderer_prepare_scene;
    tApi.prepare_view                        = pl_renderer_prepare_view;
    tApi.render_view                         = pl_renderer_render_view;
    tApi.get_view_color_texture              = pl_renderer_get_view_color_texture;
    tApi.get_view_color_texture_max_uv       = pl_renderer_get_view_color_texture_max_uv;
    tApi.resize_view                         = pl_renderer_resize_view;
    tApi.get_debug_drawlist                  = pl_renderer_get_debug_drawlist;
    tApi.get_gizmo_drawlist                  = pl_renderer_get_gizmo_drawlist;
    tApi.update_hovered_entity               = pl_renderer_update_hovered_entity;
    tApi.get_hovered_entity                  = pl_renderer_get_hovered_entity;
    tApi.get_runtime_options                 = pl_renderer_get_runtime_options;
    tApi.rebuild_scene_bvh                   = pl_renderer_rebuild_bvh;
    tApi.debug_draw_lights                   = pl_renderer_debug_draw_lights;
    tApi.debug_draw_all_bound_boxes          = pl_renderer_debug_draw_all_bound_boxes;
    tApi.debug_draw_bvh                      = pl_renderer_debug_draw_bvh;
    tApi.show_skybox                         = pl_renderer_show_skybox;
    tApi.show_grid                           = pl_renderer_show_grid;
    tApi.register_ecs_system                 = pl_renderer_register_system;
    tApi.create_skin                         = pl_renderer_create_skin;
    tApi.create_directional_light            = pl_renderer_create_directional_light;
    tApi.create_point_light                  = pl_renderer_create_point_light;
    tApi.create_spot_light                   = pl_renderer_create_spot_light;
    tApi.create_environment_probe            = pl_renderer_create_environment_probe;
    tApi.create_object                       = pl_renderer_create_object;
    tApi.copy_object                         = pl_renderer_copy_object;
    tApi.get_ecs_type_key_skin               = pl_renderer_get_ecs_type_key_skin;
    tApi.get_ecs_type_key_light              = pl_renderer_get_ecs_type_key_light;
    tApi.get_ecs_type_key_environment_probe  = pl_renderer_get_ecs_type_key_environment_probe;
    tApi.run_light_update_system             = pl_run_light_update_system;
    tApi.run_environment_probe_update_system = pl_run_probe_update_system;
    tApi.get_ecs_type_key_object             = pl_renderer_get_ecs_type_key_object;
    tApi.run_skin_update_system              = pl_run_skin_update_system;
    tApi.run_object_update_system            = pl_run_object_update_system;
    pl_set_api(ptApiRegistry, plRendererI, &tApi);

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
        gptECS              = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptCamera           = pl_get_api_latest(ptApiRegistry, plCameraI);
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
        gptTerrainProcessor = pl_get_api_latest(ptApiRegistry, plTerrainProcessorI);
        gptStage            = pl_get_api_latest(ptApiRegistry, plStageI);
        gptFreeList         = pl_get_api_latest(ptApiRegistry, plFreeListI);
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
