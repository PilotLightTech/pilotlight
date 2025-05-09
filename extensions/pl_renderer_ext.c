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

// setup/shutdown
static void     pl_refr_initialize(plRendererSettings);
static void     pl_refr_cleanup(void);

// scenes
static uint32_t pl_refr_create_scene(void);
static void     pl_refr_cleanup_scene(uint32_t);
static void     pl_add_drawable_objects_to_scene(uint32_t,  uint32_t, const plEntity*);
static void     pl_refr_add_materials_to_scene(uint32_t, uint32_t, const plEntity* atMaterials);
static void     pl_refr_remove_objects_from_scene(uint32_t sceneHandle, uint32_t objectCount, const plEntity* objects);
static void     pl_refr_update_scene_objects(uint32_t sceneHandle, uint32_t objectCount, const plEntity* objects);

// views
static uint32_t          pl_refr_create_view(uint32_t, plVec2);
static void              pl_refr_cleanup_view(uint32_t, uint32_t);
static plBindGroupHandle pl_refr_get_view_color_texture(uint32_t, uint32_t);
static void              pl_refr_resize_view(uint32_t, uint32_t, plVec2);
static void              pl_refr_resize(void);

// loading
static void pl_refr_load_skybox_from_panorama(uint32_t, const char*, int);
static void pl_refr_finalize_scene(uint32_t);
static void pl_refr_reload_scene_shaders(uint32_t);

// per frame
static void pl_refr_run_ecs(uint32_t uSceneHandle);
static void pl_refr_render_scene(uint32_t, const uint32_t*, const plViewOptions*, uint32_t);
static bool pl_refr_begin_frame(void);
static bool pl_refr_get_hovered_entity(uint32_t uSceneHandle, uint32_t uCount, plEntity*);

// misc.
static void                pl_refr_outline_entities(uint32_t, uint32_t, plEntity*);
static plComponentLibrary* pl_refr_get_component_library(uint32_t);
static plDevice*           pl_refr_get_device(void);
static plSwapchain*        pl_refr_get_swapchain(void);
static plDrawList3D*       pl_refr_get_debug_drawlist(uint32_t, uint32_t);
static plDrawList3D*       pl_refr_get_gizmo_drawlist(uint32_t, uint32_t);
static plCommandPool*      pl__refr_get_command_pool(void);

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl__refr_console_shader_reload(const char* pcName, void* pData)
{
    for(uint32_t i = 0; i < pl_sb_size(gptData->sbtScenes); i++)
        pl_refr_reload_scene_shaders(i);
}

static void
pl__refr_console_swapchain_reload(const char* pcName, void* pData)
{
    gptData->tRuntimeOptions.bReloadSwapchain = true;
}

static void
pl_refr_initialize(plRendererSettings tSettings)
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
    gptConsole->add_toggle_variable("r.FrustumCulling", &gptData->tRuntimeOptions.bFrustumCulling, "frustum culling", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable("r.DrawAllBoundBoxes", &gptData->tRuntimeOptions.bDrawAllBoundingBoxes, "draw all bounding boxes", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable("r.DrawVisibleBoundBoxes", &gptData->tRuntimeOptions.bDrawVisibleBoundingBoxes, "draw visible bounding boxes", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable("r.DrawSelectedBoundBoxes", &gptData->tRuntimeOptions.bShowSelectedBoundingBox, "draw selected bounding boxes", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable("r.ShowProbes", &gptData->tRuntimeOptions.bShowProbes, "show environment probes", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable("r.ShowOrigin", &gptData->tRuntimeOptions.bShowOrigin, "show world origin", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_float_variable("r.ShadowConstantDepthBias", &gptData->tRuntimeOptions.fShadowConstantDepthBias, "shadow constant depth bias", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_float_variable("r.ShadowSlopeDepthBias", &gptData->tRuntimeOptions.fShadowSlopeDepthBias, "shadow slope depth bias", PL_CONSOLE_VARIABLE_FLAGS_NONE | PL_CONSOLE_VARIABLE_FLAGS_READ_ONLY);
    gptConsole->add_uint_variable("r.OutlineWidth", &gptData->tRuntimeOptions.uOutlineWidth, "selection outline width", PL_CONSOLE_VARIABLE_FLAGS_NONE);
    gptConsole->add_toggle_variable_ex("r.Wireframe", &gptData->tRuntimeOptions.bWireframe, "wireframe rendering", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__refr_console_shader_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.IBL", &gptData->tRuntimeOptions.bImageBasedLighting, "image based lighting", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__refr_console_shader_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.PunctualLighting", &gptData->tRuntimeOptions.bPunctualLighting, "punctual lighting", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__refr_console_shader_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.MultiViewportShadows", &gptData->tRuntimeOptions.bMultiViewportShadows, "utilize multiviewport features", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__refr_console_shader_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.VSync", &gptData->tRuntimeOptions.bVSync, "monitor vsync", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__refr_console_swapchain_reload, NULL);
    gptConsole->add_toggle_variable_ex("r.UIMSAA", &gptData->tRuntimeOptions.bMSAA, "UI MSAA", PL_CONSOLE_VARIABLE_FLAGS_NONE, pl__refr_console_swapchain_reload, NULL);

    // add specific log channel for renderer
    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER,
        .uEntryCount = 1024
    };
    gptData->uLogChannel = gptLog->add_channel("Renderer", tLogInit);

    // default options
    gptData->pdDrawCalls = gptStats->get_counter("draw calls");
    gptData->tRuntimeOptions.bMSAA = false;
    gptData->tRuntimeOptions.bReloadMSAA = false;
    gptData->tRuntimeOptions.bVSync = true;
    gptData->uMaxTextureResolution = tSettings.uMaxTextureResolution > 0 ? tSettings.uMaxTextureResolution : 1024;
    gptData->tRuntimeOptions.uOutlineWidth = 4;
    gptData->tRuntimeOptions.bShowSelectedBoundingBox = true;
    gptData->tRuntimeOptions.bFrustumCulling = true;
    gptData->tRuntimeOptions.bImageBasedLighting = true;
    gptData->tRuntimeOptions.bPunctualLighting = true;
    gptData->tRuntimeOptions.fShadowConstantDepthBias = -1.25f;
    gptData->tRuntimeOptions.fShadowSlopeDepthBias = -10.75f;

    // shader default values
    gptData->tSkyboxShader = (plShaderHandle){0}; // is this needed still?

    gptResource->initialize((plResourceManagerInit){.ptDevice = gptData->ptDevice, .uMaxTextureResolution = tSettings.uMaxTextureResolution});

    if(gptData->tDeviceInfo.tCapabilities & PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS)
        gptData->tRuntimeOptions.bMultiViewportShadows = true;

    // create main bind group pool
    const plBindGroupPoolDesc tBindGroupPoolDesc = {
        .tFlags                      = PL_BIND_GROUP_POOL_FLAGS_INDIVIDUAL_RESET | PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING,
        .szSamplerBindings           = 100000,
        .szUniformBufferBindings     = 100000,
        .szStorageBufferBindings     = 100000,
        .szSampledTextureBindings    = 100000,
        .szStorageTextureBindings    = 100000,
        .szAttachmentTextureBindings = 100000
    };
    gptData->ptBindGroupPool = gptGfx->create_bind_group_pool(gptData->ptDevice, &tBindGroupPoolDesc);

    plBindGroupLayoutDesc tGlobalBindGroupLayout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 2,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            },
            },
            .atSamplerBindings = {
                {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
            },
            .atTextureBindings = {
                {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                {.uSlot = PL_MAX_BINDLESS_TEXTURES + 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
            }
    };

    gptData->tGlobalSceneBindGroupLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tGlobalBindGroupLayout);

    const plBindGroupLayoutDesc tLightingBindGroupLayout = {
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
        }
    };
    gptData->tLightingViewBindGroupLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tLightingBindGroupLayout);

    const plBindGroupLayoutDesc tPickBindGroupLayout = {
        .atBufferBindings = { 
            {.uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
        }
    };
    gptData->tPickViewBindGroupLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tPickBindGroupLayout);

    plBindGroupLayoutDesc tComputeBindGroupLayout = {
        .atBufferBindings = {
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_SHADER_STAGE_COMPUTE},
        },
    };
    gptData->tSkyboxComputeBindGroupLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tComputeBindGroupLayout);


    plBindGroupLayoutDesc tSkyboxBindGroupLayout = {
        .atTextureBindings = { {.uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}}
    };
    gptData->tSkyboxBindGroupLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tSkyboxBindGroupLayout);

    const plBindGroupLayoutDesc tSkyboxBG0Layout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        },
        .atSamplerBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
        }
    };
    gptData->tSkyboxBG0Layout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tSkyboxBG0Layout);

    plBindGroupLayoutDesc tSkyboxBG1Layout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        },
        .atSamplerBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
        }
    };
    gptData->tSkyboxBG1Layout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tSkyboxBG1Layout);


    const plBindGroupLayoutDesc tDeferredBG1Layout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        }
    };
    gptData->tDeferredBG1Layout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tDeferredBG1Layout);

    const plBindGroupLayoutDesc tSceneBGLayout = {
        .atBufferBindings = {
            { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 3, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
            { .uSlot = 4, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_FRAGMENT | PL_SHADER_STAGE_VERTEX},
        },
        .atSamplerBindings = {
            {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
        },
    };
    gptData->tSceneBGLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tSceneBGLayout);

    const plBindGroupLayoutDesc tPickBG0Layout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        }
    };
    gptData->tPickBG0Layout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tPickBG0Layout);

    const plBindGroupLayoutDesc tJFABindGroupLayout = {
        .atTextureBindings = {
            {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE}
        }
    };
    gptData->tJFABGLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tJFABindGroupLayout);

    const plBindGroupLayoutDesc tSkinBindGroupLayout0 = {
        .atSamplerBindings = {
            {.uSlot =  3, .tStages = PL_SHADER_STAGE_COMPUTE}
        },
        .atBufferBindings = {
            { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_COMPUTE},
        }
    };
    gptData->tSkin2BGLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tSkinBindGroupLayout0);


    plBindGroupLayoutDesc tBindGroupLayout0 = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1,
                .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
            }
        }
    };
    gptData->tShadowGlobalBGLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tBindGroupLayout0);

    const plBindGroupLayoutDesc tOutlineBindGroupLayout = {
        .atSamplerBindings = {
            { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
            {.uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        }
    };
    gptData->tOutlineBGLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tOutlineBindGroupLayout);

    plBindGroupLayoutDesc tSkinBindGroupLayout = {
        .atBufferBindings = {
            {.uSlot =  0, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_BUFFER_BINDING_TYPE_STORAGE}
        }
    };
    gptData->tSkinBGLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tSkinBindGroupLayout);

    plBindGroupLayoutDesc tEnvBindgroupLayout = {
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        },
        .atBufferBindings = {
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 7, .tStages = PL_SHADER_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 8, .tStages = PL_SHADER_STAGE_COMPUTE},
        },
        .atSamplerBindings = { {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE}}
    };
    gptData->tEnvBGLayout = gptGfx->create_bind_group_layout(gptData->ptDevice, &tEnvBindgroupLayout);


    // create pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptData->atCmdPools[i] = gptGfx->create_command_pool(gptData->ptDevice, NULL);
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

    // create staging buffers
    // const plBufferDesc tStagingBufferDesc = {
    //     .tUsage      = PL_BUFFER_USAGE_STAGING,
    //     .szByteSize  = 268435456,
    //     .pcDebugName = "Renderer Staging Buffer"
    // };
    // for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    // {
    //     gptData->atStagingBufferHandle[i].tStagingBufferHandle = pl__refr_create_staging_buffer(&tStagingBufferDesc, "staging", i);
    //     gptData->atStagingBufferHandle[i].szOffset = 0;
    //     gptData->atStagingBufferHandle[i].szSize = tStagingBufferDesc.szByteSize;
    // }

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
    gptData->tDummyTexture = pl__refr_create_texture_with_data(&tDummyTextureDesc, "dummy", 0, afDummyTextureData, sizeof(afDummyTextureData));

    const plTextureDesc tSkyboxTextureDesc = {
        .tDimensions = {1, 1, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 6,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_CUBE,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "dummy cube"
    };
    gptData->tDummyTextureCube = pl__refr_create_texture(&tSkyboxTextureDesc, "dummy cube", 0, PL_TEXTURE_USAGE_SAMPLED);

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

    // create template shaders

    pl_refr_create_global_shaders();

    const plComputeShaderDesc tComputeShaderDesc = {
        .tShader = gptShader->load_glsl("jumpfloodalgo.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE}
                }
            }
        }
    };
    gptData->tJFAShader = gptGfx->create_compute_shader(gptData->ptDevice, &tComputeShaderDesc);
    pl_temp_allocator_reset(&gptData->tTempAllocator);

    // create full quad
    const uint32_t auFullQuadIndexBuffer[] = {0, 1, 2, 0, 2, 3};
    const plBufferDesc tFullQuadIndexBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_INDEX,
        .szByteSize = sizeof(uint32_t) * 6,
        .pcDebugName = "Renderer Quad Index Buffer"
    };
    gptData->tFullQuadIndexBuffer = pl__refr_create_local_buffer(&tFullQuadIndexBufferDesc, "full quad index buffer", 0, auFullQuadIndexBuffer, tFullQuadIndexBufferDesc.szByteSize);

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
    gptData->tFullQuadVertexBuffer = pl__refr_create_local_buffer(&tFullQuadVertexBufferDesc, "full quad vertex buffer", 0, afFullQuadVertexBuffer, tFullQuadVertexBufferDesc.szByteSize);

    // create semaphores
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptData->aptSemaphores[i] = gptGfx->create_semaphore(gptData->ptDevice, false);
    gptData->ptClickSemaphore = gptGfx->create_semaphore(gptData->ptDevice, false);

    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat, .tSamples = PL_SAMPLE_COUNT_1},
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0}
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
    gptData->tMainRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tMainRenderPassLayoutDesc);

    const plRenderPassLayoutDesc tMainMSAARenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat, .bResolve = true }, // swapchain
            { .tFormat = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat, .tSamples = gptData->tDeviceInfo.tMaxSampleCount}, // msaa
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
    gptData->tMainMSAARenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tMainMSAARenderPassLayoutDesc);

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptData->ptSwap, &uImageCount);

    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
    }
    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptData->ptSwap);
    const plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = gptData->tMainRenderPassLayout,
        .atColorTargets = { // msaa
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {(float)tInfo.uWidth, (float)tInfo.uHeight},
        .ptSwapchain = gptData->ptSwap
    };
    gptData->tMainRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);
    gptData->tCurrentMainRenderPass = gptData->tMainRenderPass;

    plComputeShaderDesc tFilterComputeShaderDesc = {
        .tShader = gptShader->load_glsl("filter_environment.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                },
                .atBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 7, .tStages = PL_SHADER_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 8, .tStages = PL_SHADER_STAGE_COMPUTE},
                },
                .atSamplerBindings = { {.uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE}}
            }
        }
    };
    gptData->tEnvFilterShader = gptGfx->create_compute_shader(gptData->ptDevice, &tFilterComputeShaderDesc);
};

static uint32_t
pl_refr_create_scene(void)
{
    uint32_t uSceneHandle = UINT32_MAX;
    if(pl_sb_size(gptData->sbuSceneFreeIndices) > 0)
    {
        uSceneHandle = pl_sb_pop(gptData->sbuSceneFreeIndices);
    }
    else
    {
        uSceneHandle = pl_sb_size(gptData->sbtScenes);
        pl_sb_add(gptData->sbtScenes);
    }
    plRefScene tScene = {
        .bActive                    = true,
        .tIndexBuffer               = {.uData = UINT32_MAX},
        .tVertexBuffer              = {.uData = UINT32_MAX},
        .tStorageBuffer             = {.uData = UINT32_MAX},
        .tSkinStorageBuffer         = {.uData = UINT32_MAX},
        .uGPUMaterialBufferCapacity = 512
    };
    gptData->sbtScenes[uSceneHandle] = tScene;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // initialize ecs library
    gptECS->init_component_library(&ptScene->tComponentLibrary);

    // create global bindgroup
    ptScene->uTextureIndexCount = 0;


    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        const plBindGroupDesc tGlobalBindGroupDesc = {
            .ptPool      = gptData->ptBindGroupPool,
            .tLayout     = gptData->tGlobalSceneBindGroupLayout,
            .pcDebugName = "global bind group"
        };
        ptScene->atGlobalBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBindGroupDesc);

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
            .uSamplerCount = 2,
            .atSamplerBindings = tGlobalSamplerData,
        };

        gptGfx->update_bind_group(gptData->ptDevice, ptScene->atGlobalBindGroup[i], &tGlobalBindGroupData);
    }

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        // .szByteSize = 134217728,
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

        ptScene->atLightShadowDataBuffer[i] = pl__refr_create_staging_buffer(&atLightShadowDataBufferDesc, "shadow buffer", i);
        ptScene->atShadowCameraBuffers[i] = pl__refr_create_staging_buffer(&atCameraBuffersDesc, "shadow camera buffer", i);

        ptScene->atGPUProbeDataBuffers[i] = pl__refr_create_staging_buffer(&atProbeDataBufferDesc, "probe buffer", i);

    }

    for(uint32_t i = 0; i < 7; i++)
    {
        const size_t uMaxFaceSize = ((size_t)1024 * (size_t)1024) * 4 * sizeof(float);

        const plBufferDesc tInputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uMaxFaceSize,
            .pcDebugName = "filter buffers"
        };
        ptScene->atFilterWorkingBuffers[i] = pl__refr_create_local_buffer(&tInputBufferDesc, "filter buffer", i, NULL, 0);
    }

    plMaterialComponent* ptMaterial = NULL;
    plEntity tMaterial = gptECS->create_material(&ptScene->tComponentLibrary, "environment probe material", &ptMaterial);
    ptMaterial->tBlendMode = PL_BLEND_MODE_OPAQUE;
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tFlags = PL_MATERIAL_FLAG_CAST_RECEIVE_SHADOW;
    ptMaterial->tBaseColor = (plVec4){1.0f, 1.0f, 1.0f, 1.0f};
    ptMaterial->fRoughness = 0.0f;
    ptMaterial->fMetalness = 1.0f;

    plMeshComponent* ptMesh = NULL;
    ptScene->tProbeMesh = gptECS->create_sphere_mesh(&ptScene->tComponentLibrary, "environment probe mesh", 0.25f, 32, 32, &ptMesh);
    ptMesh->tMaterial = tMaterial;

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
    ptScene->tShadowTexture = pl__refr_create_local_texture(&tShadowDepthTextureDesc, "shadow map", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptScene->atShadowTextureBindlessIndices = pl__get_bindless_texture_index(uSceneHandle, ptScene->tShadowTexture);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        atShadowAttachmentSets[i].atViewAttachments[0] = ptScene->tShadowTexture;
    }

    ptScene->tShadowRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tDepthRenderPassDesc, atShadowAttachmentSets);
    ptScene->tFirstShadowRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tFirstDepthRenderPassDesc, atShadowAttachmentSets);

    const plShaderDesc tTonemapShaderDesc = {
        .tPixelShader = gptShader->load_glsl("tonemap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("full_quad.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 4,
                .atAttributes = {
                    {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT2},
                    {.uByteOffset = sizeof(float) * 2, .tFormat = PL_VERTEX_FORMAT_FLOAT2}
                }
            }
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tPostProcessRenderPassLayout,
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    ptScene->tTonemapShader = gptGfx->create_shader(gptData->ptDevice, &tTonemapShaderDesc);

    return uSceneHandle;
}

static void
pl_refr_cleanup_view(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];

    pl_sb_free(ptView->sbtVisibleDrawables);
    pl_sb_free(ptView->sbtVisibleOpaqueDrawables);
    pl_sb_free(ptView->sbtVisibleTransparentDrawables);
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

    *ptView = (plRefView){0};
}

static void
pl_refr_cleanup_scene(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    if(!ptScene->bActive)
        return;

    for(uint32_t j = 0; j < ptScene->uViewCount; j++)
        pl_refr_cleanup_view(uSceneHandle, j);

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
            pl_sb_free(ptProbe->sbtVisibleOpaqueDrawables[k]);
            pl_sb_free(ptProbe->sbtVisibleTransparentDrawables[k]);
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
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tTonemapShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tLightingShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tEnvLightingShader);

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
    pl_sb_free(ptScene->sbtStagedDrawables);
    pl_sb_free(ptScene->sbtSkinData);
    pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
    pl_sb_free(ptScene->sbtOutlineDrawables);
    pl_sb_free(ptScene->sbtOutlineDrawablesOldShaders);
    pl_sb_free(ptScene->sbtOutlineDrawablesOldEnvShaders);
    pl_hm_free(&ptScene->tDrawableHashmap);
    pl_hm_free(&ptScene->tMaterialHashmap);
    pl_hm_free(&ptScene->tTextureIndexHashmap);
    pl_hm_free(&ptScene->tCubeTextureIndexHashmap);
    gptECS->cleanup_component_library(&ptScene->tComponentLibrary);

    pl_sb_push(gptData->sbuSceneFreeIndices, uSceneHandle);
    plRefScene tScene = {
        .bActive                    = false,
        .tIndexBuffer               = {.uData = UINT32_MAX},
        .tVertexBuffer              = {.uData = UINT32_MAX},
        .tStorageBuffer             = {.uData = UINT32_MAX},
        .tSkinStorageBuffer         = {.uData = UINT32_MAX},
        .uGPUMaterialBufferCapacity = 512,
        .uViewCount                 = 0
    };
    *ptScene = tScene;
}

static uint32_t
pl_refr_create_view(uint32_t uSceneHandle, plVec2 tDimensions)
{

    // for convience
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // create view
    const uint32_t uViewHandle = ptScene->uViewCount++;
    PL_ASSERT(uViewHandle < PL_MAX_VIEWS_PER_SCENE);    
    plRefView* ptView = &ptScene->atViews[uViewHandle];

    ptView->tTargetSize = tDimensions;

    // picking defaults
    ptView->tHoveredEntity.ulData = 0;
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

    ptView->tRawOutputTexture        = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen raw", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tAlbedoTexture           = pl__refr_create_texture(&tAlbedoTextureDesc, "albedo original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tNormalTexture           = pl__refr_create_texture(&tNormalTextureDesc, "normal original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tAOMetalRoughnessTexture = pl__refr_create_texture(&tEmmissiveTexDesc, "metalroughness original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tDepthTexture            = pl__refr_create_texture(&tDepthTextureDesc, "offscreen depth original", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->atUVMaskTexture0         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 0", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->atUVMaskTexture1         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 1", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->tFinalTexture            = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen final", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tFinalTextureHandle      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture);

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .tLayout     = gptData->tLightingViewBindGroupLayout,
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

    ptView->tPickTexture = pl__refr_create_texture(&tPickTextureDesc, "pick original", 0, PL_TEXTURE_USAGE_SAMPLED);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

        const plBufferDesc tPickBufferDesc = {
            .tUsage     = PL_BUFFER_USAGE_STAGING | PL_BUFFER_USAGE_STORAGE,
            .szByteSize = sizeof(uint32_t) * 2,
            .pcDebugName = "Picking buffer"
        };
        ptView->atPickBuffer[i] = pl__refr_create_cached_staging_buffer(&tPickBufferDesc, "picking buffer", 0);

        const plBindGroupDesc tPickBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .tLayout = gptData->tPickViewBindGroupLayout,
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
        ptView->atGlobalBuffers[i] = pl__refr_create_staging_buffer(&atGlobalBuffersDesc, "global", i);
        

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

        ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[i] = pl__refr_create_staging_buffer(&atLightShadowDataBufferDesc, "d shadow", i);
        ptView->tDirectionLightShadowData.atDShadowCameraBuffers[i] = pl__refr_create_staging_buffer(&atCameraBuffersDesc, "d shadow buffer", i);

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

    return uViewHandle;
}

static void
pl_refr_resize_view(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions)
{
    // for convience
    plDevice*   ptDevice = gptData->ptDevice;
    plRefScene* ptScene  = &gptData->sbtScenes[uSceneHandle];
    plRefView*  ptView   = &ptScene->atViews[uViewHandle];

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
    ptView->tPickTexture = pl__refr_create_texture(&tPickTextureDesc, "pick", 0, PL_TEXTURE_USAGE_SAMPLED);

    // textures
    ptView->tRawOutputTexture        = pl__refr_create_texture(&tRawOutputTextureDesc, "offscreen raw", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tAlbedoTexture           = pl__refr_create_texture(&tAlbedoTextureDesc, "albedo original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tNormalTexture           = pl__refr_create_texture(&tNormalTextureDesc, "normal resize", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tAOMetalRoughnessTexture = pl__refr_create_texture(&tEmmissiveTexDesc, "metalroughness original", 0, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
    ptView->tDepthTexture            = pl__refr_create_texture(&tDepthTextureDesc, "offscreen depth original", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->atUVMaskTexture0         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 0", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->atUVMaskTexture1         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 1", 0, PL_TEXTURE_USAGE_STORAGE);
    ptView->tFinalTexture            = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen final", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tFinalTextureHandle      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture);

    // lighting bind group
    const plBindGroupDesc tLightingBindGroupDesc = {
        .ptPool = gptData->ptBindGroupPool,
        .tLayout = gptData->tLightingViewBindGroupLayout,
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

static void
pl_refr_cleanup(void)
{
    pl_temp_allocator_free(&gptData->tTempAllocator);
    gptGfx->cleanup_draw_stream(&gptData->tDrawStream);
    
    for(uint32_t i = 0; i < pl_sb_size(gptData->sbtScenes); i++)
        pl_refr_cleanup_scene(i);

    for(uint32_t i = 0; i < pl_sb_size(gptData->_sbtVariantHandles); i++)
    {
        plShader* ptShader = gptGfx->get_shader(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
        gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
    }
    pl_sb_free(gptData->sbuSceneFreeIndices);
    pl_sb_free(gptData->_sbtVariantHandles);
    pl_hm_free(&gptData->tVariantHashmap);
    gptResource->cleanup();
    gptGfx->flush_device(gptData->ptDevice);

    gptGfx->destroy_texture(gptData->ptDevice, gptData->tMSAATexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->cleanup_semaphore(gptData->aptSemaphores[i]);
    gptGfx->cleanup_semaphore(gptData->ptClickSemaphore);

    gptGfx->cleanup_bind_group_pool(gptData->ptBindGroupPool);
    gptGpuAllocators->cleanup(gptData->ptDevice);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_bind_group_pool(gptData->aptTempGroupPools[i]);
        gptGfx->cleanup_command_pool(gptData->atCmdPools[i]);
    }

    

    // must be cleaned up after graphics since 3D drawlist are registered as pointers
    pl_sb_free(gptData->sbtScenes);
    PL_FREE(gptData);
}

static plComponentLibrary*
pl_refr_get_component_library(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    return &ptScene->tComponentLibrary;
}

static plDevice*
pl_refr_get_device(void)
{
    return gptData->ptDevice;
}

static plSwapchain*
pl_refr_get_swapchain(void)
{
    return gptData->ptSwap;
}

static void
pl_refr_load_skybox_from_panorama(uint32_t uSceneHandle, const char* pcPath, int iResolution)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    const int iSamples = 512;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice* ptDevice = gptData->ptDevice;
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    ptScene->tRuntimeOptions.bShowSkybox = true;

    // create skybox shader if we haven't
    if(gptData->tSkyboxShader.uIndex == 0)
    {
        // create skybox shader
        plShaderDesc tSkyboxShaderDesc = {
            .tPixelShader = gptShader->load_glsl("skybox.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("skybox.vert", "main", NULL, NULL),
            .tGraphicsState = {
                .ulDepthWriteEnabled  = 0,
                .ulDepthMode          = PL_COMPARE_MODE_EQUAL,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 3,
                    .atAttributes = { {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT3}}
                }
            },
            .atBlendStates = {
                pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
            },
            .tRenderPassLayout = gptData->tRenderPassLayout,
            .uSubpassIndex = 2,
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                    },
                    .atSamplerBindings = {
                        {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    },
                    .atTextureBindings = {
                        {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 6, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 7, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                    },
                }
            }
        };
        gptData->tSkyboxShader = gptGfx->create_shader(gptData->ptDevice, &tSkyboxShaderDesc);
    }

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    pl_begin_cpu_sample(gptProfile, 0, "load image");
    size_t szImageFileSize = 0;
    gptFile->binary_read(pcPath, &szImageFileSize, NULL);
    unsigned char* pucBuffer = PL_ALLOC(szImageFileSize);
    gptFile->binary_read(pcPath, &szImageFileSize, pucBuffer);
    float* pfPanoramaData = gptImage->load_hdr(pucBuffer, (int)szImageFileSize, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
    PL_FREE(pucBuffer);
    pl_end_cpu_sample(gptProfile, 0);
    PL_ASSERT(pfPanoramaData);

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    {
        int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
        const plComputeShaderDesc tSkyboxComputeShaderDesc = {
            .tShader = gptShader->load_glsl("panorama_to_cubemap.comp", "main", NULL, NULL),
            .pTempConstantData = aiSkyboxSpecializationData,
            .atConstants = {
                { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
                { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
                { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT}
            },
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_SHADER_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_SHADER_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_SHADER_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_SHADER_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_SHADER_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_SHADER_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_SHADER_STAGE_COMPUTE},
                    }
                }
            }
        };
        plComputeShaderHandle tPanoramaShader = gptGfx->create_compute_shader(ptDevice, &tSkyboxComputeShaderDesc);
        pl_temp_allocator_reset(&gptData->tTempAllocator);

        plBufferHandle atComputeBuffers[7] = {0};
        const uint32_t uPanoramaSize = iPanoramaHeight * iPanoramaWidth * 4 * sizeof(float);
        const plBufferDesc tInputBufferDesc = {
            .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
            .szByteSize = uPanoramaSize,
            .pcDebugName = "panorama input buffer"
        };
        atComputeBuffers[0] = pl__refr_create_staging_buffer(&tInputBufferDesc, "panorama input", 0);
        plBuffer* ptComputeBuffer = gptGfx->get_buffer(ptDevice, atComputeBuffers[0]);
        memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, uPanoramaSize);
        
        gptImage->free(pfPanoramaData);

        const plBufferDesc tOutputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE,
            .szByteSize = uFaceSize,
            .pcDebugName = "panorama output buffer"
        };
        
        for(uint32_t i = 0; i < 6; i++)
            atComputeBuffers[i + 1] = pl__refr_create_local_buffer(&tOutputBufferDesc, "panorama output", i, NULL, 0);

        const plBindGroupDesc tComputeBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .tLayout     = gptData->tSkyboxComputeBindGroupLayout,
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
        ptScene->tSkyboxTexture = pl__refr_create_texture(&tSkyboxTextureDesc, "skybox texture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

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
            .tLayout = gptData->tSkyboxBindGroupLayout,
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

        pl__refr_add_skybox_drawable(uSceneHandle);
    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_outline_entities(uint32_t uSceneHandle, uint32_t uCount, plEntity* atEntities)
{
    // for convience
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice = gptData->ptDevice;

    int iSceneWideRenderingFlags = 0;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // reset old entities
    const uint32_t uOldSelectedEntityCount = pl_sb_size(ptScene->sbtOutlineDrawables);
    for(uint32_t i = 0; i < uOldSelectedEntityCount; i++)
    {
        plEntity tEntity = ptScene->sbtOutlineDrawables[i].tEntity;
        plShader* ptOutlineShader = gptGfx->get_shader(ptDevice, ptScene->sbtOutlineDrawables[i].tShader);

        plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

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
        int aiConstantData0[5] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iObjectRenderingFlags
        };

        // use stencil buffer
        const plGraphicsState tOutlineVariantTemp = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_CULL_FRONT,
            .ulWireframe          = 0,
            .ulStencilTestEnabled = 1,
            .ulStencilMode        = PL_COMPARE_MODE_LESS,
            .ulStencilRef         = 128,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        };

        const plShaderVariant tOutlineVariant = {
            .pTempConstantData = aiConstantData0,
            .tGraphicsState    = tOutlineVariantTemp
        };

        // size_t szSpecializationSize = 0;
        // for(uint32_t j = 0; j < ptOutlineShader->tDesc._uConstantCount; j++)
        // {
        //     const plSpecializationConstant* ptConstant = &ptOutlineShader->tDesc.atConstants[j];
        //     szSpecializationSize += pl__get_data_type_size2(ptConstant->tType);
        // }

        // const uint64_t ulVariantHash = pl_hm_hash(tOutlineVariant.pTempConstantData, szSpecializationSize, tOutlineVariant.tGraphicsState.ulValue);
        // pl_hm_remove(&gptData->ptVariantHashmap, ulVariantHash);

        uint64_t ulIndex = 0;
        if(pl_hm_has_key_ex(&ptScene->tDrawableHashmap, tEntity.ulData, &ulIndex))
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

        // gptGfx->queue_shader_for_deletion(ptDevice, ptScene->sbtOutlineDrawables[i].tShader);
    }
    pl_sb_reset(ptScene->sbtOutlineDrawables);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldShaders);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldEnvShaders);

    for(uint32_t i = 0; i < uCount; i++)
    {
        plEntity tEntity = atEntities[i];

        plObjectComponent* ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        if(ptObject == NULL)
            continue;
        plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

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
        if(pl_hm_has_key_ex(&ptScene->tDrawableHashmap, tEntity.ulData, &ulIndex))
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

            // use stencil buffer
            const plGraphicsState tOutlineVariantTemp = {
                .ulDepthWriteEnabled  = 0,
                .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .ulCullMode           = PL_CULL_MODE_CULL_FRONT,
                .ulWireframe          = 0,
                .ulStencilTestEnabled = 1,
                .ulStencilMode        = PL_COMPARE_MODE_LESS,
                .ulStencilRef         = 128,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            };

            const plShaderVariant tOutlineVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tOutlineVariantTemp
            };

            pl_sb_push(ptScene->sbtOutlineDrawables, *ptDrawable);

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            pl_sb_push(ptScene->sbtOutlineDrawablesOldShaders, ptDrawable->tShader);
            pl_sb_push(ptScene->sbtOutlineDrawablesOldEnvShaders, ptDrawable->tEnvShader);

            if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_FORWARD)
                ptDrawable->tShader = pl__get_shader_variant(uSceneHandle, gptData->tForwardShader, &tVariant);
            else if(ptDrawable->tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                ptDrawable->tShader = pl__get_shader_variant(uSceneHandle, gptData->tDeferredShader, &tVariant);

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

static void
pl_refr_reload_scene_shaders(uint32_t uSceneHandle)
{
    // fill CPU buffers & drawable list
    pl_begin_cpu_sample(gptProfile, 0, "recreate shaders");

    gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_CYAN, 1.0f, "%s", "reloaded shaders");

    // old cleanup
    for(uint32_t i = 0; i < pl_sb_size(gptData->_sbtVariantHandles); i++)
    {
        plShader* ptShader = gptGfx->get_shader(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
        gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
    }
    pl_sb_free(gptData->_sbtVariantHandles);
    pl_hm_free(&gptData->tVariantHashmap);

    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tDeferredShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tForwardShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tAlphaShadowShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tShadowShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tPickShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tUVShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tSkyboxShader);
    gptGfx->queue_compute_shader_for_deletion(gptData->ptDevice, gptData->tEnvFilterShader);

    pl_refr_create_global_shaders();

    if(uSceneHandle >= pl_sb_size(gptData->sbtScenes))
    {
        pl_end_cpu_sample(gptProfile, 0);
        return;
    }

    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    if(!ptScene->bActive)
    {
        pl_end_cpu_sample(gptProfile, 0);
        return;
    }

    plDevice* ptDevice = gptData->ptDevice;



    pl_log_info_f(gptLog, gptData->uLogChannel, "reload shaders for scene %u", uSceneHandle);

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

    pl_sb_reset(ptScene->sbtOutlineDrawables);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldShaders);
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldEnvShaders);

    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tLightingShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tTonemapShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tEnvLightingShader);

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // create lighting shader
    {
        const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;

        int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_capacity(ptScene->sbtLightData), pl_sb_size(ptScene->sbtProbeData)};
        plShaderDesc tLightingShaderDesc = {
            .tPixelShader = gptShader->load_glsl("deferred_lighting.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("deferred_lighting.vert", "main", NULL, NULL),
            .tGraphicsState = {
                .ulDepthWriteEnabled  = 0,
                .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
            },
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 4,
                    .atAttributes = {
                        {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT2},
                        {.uByteOffset = sizeof(float) * 2, .tFormat = PL_VERTEX_FORMAT_FLOAT2}
                    }
                }
            },
            .atBlendStates = {
                pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
            },
            .pTempConstantData = aiLightingConstantData,
            .uSubpassIndex = 1,
            .tRenderPassLayout = gptData->tRenderPassLayout,
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 0,
                            .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                        },
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 1,
                            .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                        },
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 2,
                            .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                        }
                    },
                    .atSamplerBindings = {
                        {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                    },
                    .atTextureBindings = {
                        {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                        {.uSlot = PL_MAX_BINDLESS_TEXTURES + 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 3, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
                    },
                },
                {
                    .atBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 3, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 4, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                    },
                    .atSamplerBindings = {
                        {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                    },
                }
            }
        };
        for(uint32_t i = 0; i < 3; i++)
        {
            tLightingShaderDesc.atConstants[i].uID = i;
            tLightingShaderDesc.atConstants[i].uOffset = i * sizeof(int);
            tLightingShaderDesc.atConstants[i].tType = PL_DATA_TYPE_INT;
        }
        ptScene->tLightingShader = gptGfx->create_shader(gptData->ptDevice, &tLightingShaderDesc);
        aiLightingConstantData[0] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
        ptScene->tEnvLightingShader = gptGfx->create_shader(gptData->ptDevice, &tLightingShaderDesc);
    }

    const plShaderDesc tTonemapShaderDesc = {
        .tPixelShader = gptShader->load_glsl("tonemap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("full_quad.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 4,
                .atAttributes = {
                    {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT2},
                    {.uByteOffset = sizeof(float) * 2, .tFormat = PL_VERTEX_FORMAT_FLOAT2}
                }
            }
        },
        .atBlendStates = {
            pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
        },
        .tRenderPassLayout = gptData->tPostProcessRenderPassLayout,
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    {.uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    ptScene->tTonemapShader = gptGfx->create_shader(gptData->ptDevice, &tTonemapShaderDesc);

    pl__refr_set_drawable_shaders(uSceneHandle);
    pl__refr_sort_drawables(uSceneHandle);

    gptShader->set_options(&tOriginalOptions);
    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_finalize_scene(uint32_t uSceneHandle)
{
    // for convience
    plRefScene* ptScene  = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice = gptData->ptDevice;

    plEntity* sbtProbes = ptScene->tComponentLibrary.tEnvironmentProbeCompManager.sbtEntities;
    for(uint32_t i = 0; i < pl_sb_size(sbtProbes); i++)
        pl__create_probe_data(uSceneHandle, sbtProbes[i]);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plMaterialComponent* sbtMaterials = ptScene->tComponentLibrary.tMaterialComponentManager.pComponents;
    const uint32_t uMaterialCount = pl_sb_size(sbtMaterials);
    pl_refr_add_materials_to_scene(uSceneHandle, uMaterialCount, ptScene->tComponentLibrary.tMaterialComponentManager.sbtEntities);

    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    pl_sb_reserve(ptScene->sbtVertexDataBuffer, 40000000);
    pl_sb_reserve(ptScene->sbtVertexPosBuffer, 15000000);
    int iLightCount = (int)pl_sb_size(sbtLights);

    pl_sb_reserve(ptScene->sbtLightData, iLightCount);

    pl__refr_unstage_drawables(uSceneHandle);
    pl__refr_set_drawable_shaders(uSceneHandle);
    pl__refr_sort_drawables(uSceneHandle);

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
        ptScene->atLightBuffer[i] = pl__refr_create_staging_buffer(&tLightBufferDesc, "light", i);
        ptScene->atTransformBuffer[i] = pl__refr_create_staging_buffer(&tTransformBufferDesc, "transform", i);
        ptScene->atInstanceBuffer[i] = pl__refr_create_staging_buffer(&tInstanceBufferDesc, "instance", i);
        ptScene->atMaterialDataBuffer[i] = pl__refr_create_local_buffer(&tMaterialDataBufferDesc,  "material buffer", uSceneHandle, ptScene->sbtMaterialBuffer, pl_sb_size(ptScene->sbtMaterialBuffer) * sizeof(plGPUMaterial));
    }

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_SHADOWS;
    if(gptData->tRuntimeOptions.bPunctualLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(gptData->tRuntimeOptions.bImageBasedLighting)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // create lighting shader
    {
        int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_capacity(ptScene->sbtLightData), pl_sb_size(ptScene->sbtProbeData)};
        plShaderDesc tLightingShaderDesc = {
            .tPixelShader = gptShader->load_glsl("deferred_lighting.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("deferred_lighting.vert", "main", NULL, NULL),
            .tGraphicsState = {
                .ulDepthWriteEnabled  = 0,
                .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
                .ulCullMode           = PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 4,
                    .atAttributes = {
                        {.uByteOffset = 0, .tFormat = PL_VERTEX_FORMAT_FLOAT2},
                        {.uByteOffset = sizeof(float) * 2, .tFormat = PL_VERTEX_FORMAT_FLOAT2}
                    }
                }
            },
            .atBlendStates = {
                pl__get_blend_state(PL_BLEND_MODE_OPAQUE)
            },
            .pTempConstantData = aiLightingConstantData,
            .uSubpassIndex = 1,
            .tRenderPassLayout = gptData->tRenderPassLayout,
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 0,
                            .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                        },
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 1,
                            .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                        },
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 2,
                            .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT
                        },
                    },
                    .atSamplerBindings = {
                        {.uSlot = 3, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        {.uSlot = 4, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                    },
                    .atTextureBindings = {
                        {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                        {.uSlot = PL_MAX_BINDLESS_TEXTURES + 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 2, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 3, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
                    },
                },
                {
                    .atBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 3, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                        { .uSlot = 4, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT},
                    },
                    .atSamplerBindings = {
                        {.uSlot = 5, .tStages = PL_SHADER_STAGE_VERTEX | PL_SHADER_STAGE_FRAGMENT}
                    },
                }
            }
        };
        for(uint32_t i = 0; i < 3; i++)
        {
            tLightingShaderDesc.atConstants[i].uID = i;
            tLightingShaderDesc.atConstants[i].uOffset = i * sizeof(int);
            tLightingShaderDesc.atConstants[i].tType = PL_DATA_TYPE_INT;
        }
        ptScene->tLightingShader = gptGfx->create_shader(gptData->ptDevice, &tLightingShaderDesc);
        aiLightingConstantData[0] = gptData->tRuntimeOptions.bPunctualLighting ? (PL_RENDERING_FLAG_USE_PUNCTUAL | PL_RENDERING_FLAG_SHADOWS) : 0;
        ptScene->tEnvLightingShader = gptGfx->create_shader(gptData->ptDevice, &tLightingShaderDesc);
    }

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

static void
pl_refr_run_ecs(uint32_t uSceneHandle)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    gptECS->run_script_update_system(&ptScene->tComponentLibrary);
    gptECS->run_animation_update_system(&ptScene->tComponentLibrary, gptIOI->get_io()->fDeltaTime);
    gptPhysics->update(gptIOI->get_io()->fDeltaTime, &ptScene->tComponentLibrary);
    gptECS->run_transform_update_system(&ptScene->tComponentLibrary);
    gptECS->run_hierarchy_update_system(&ptScene->tComponentLibrary);
    gptECS->run_light_update_system(&ptScene->tComponentLibrary);
    gptECS->run_camera_update_system(&ptScene->tComponentLibrary);
    gptECS->run_inverse_kinematics_update_system(&ptScene->tComponentLibrary);
    gptECS->run_skin_update_system(&ptScene->tComponentLibrary);
    gptECS->run_object_update_system(&ptScene->tComponentLibrary);
    gptECS->run_environment_probe_update_system(&ptScene->tComponentLibrary); // run after object update
    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_update_hovered_entity(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tOffset, plVec2 tWindowScale)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    ptView->bRequestHoverCheck = true;
    ptView->tHoverOffset = tOffset;
    ptView->tHoverWindowRatio = tWindowScale;
}

static bool
pl_refr_get_hovered_entity(uint32_t uSceneHandle, uint32_t uViewHandle, plEntity* ptEntityOut)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    if(ptEntityOut)
        *ptEntityOut = ptView->tHoveredEntity;
    bool bNewValue = ptView->auHoverResultReady[gptGfx->get_current_frame_index()];
    ptView->auHoverResultReady[gptGfx->get_current_frame_index()] = false;
    return bNewValue;
}

static void
pl_refr_render_scene(uint32_t uSceneHandle, const uint32_t* auViewHandles, const plViewOptions* atOptions, uint32_t uViewCount)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptData->atCmdPools[uFrameIdx];
    plDevice*      ptDevice  = gptData->ptDevice;
    plDrawStream*  ptStream  = &gptData->tDrawStream;
    plRefScene*    ptScene   = &gptData->sbtScenes[uSceneHandle];

    //~~~~~~~~~~~~~~~~~~~~~~transform & instance buffer update~~~~~~~~~~~~~~~~~~~~~

    // update transform & instance buffers since we are now using indices
    plBuffer* ptTransformBuffer = gptGfx->get_buffer(ptDevice, ptScene->atTransformBuffer[uFrameIdx]);
    plBuffer* ptInstanceBuffer = gptGfx->get_buffer(ptDevice, ptScene->atInstanceBuffer[uFrameIdx]);

    uint32_t uInstanceOffset = 0;
    const uint32_t uObjectCount = pl_sb_size(ptScene->sbtDrawables);
    for(uint32_t i = 0; i < uObjectCount; i++)
    {

        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtDrawables[i].tEntity);

        // copy transform into proper location in CPU side buffer
        plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
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
        .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
        .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
    };

    plCommandBuffer* ptSkinningCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptSkinningCmdBuffer, &tSkinningBeginInfo);

    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, ptScene->sbtSkinData[i].atDynamicSkinBuffer[uFrameIdx]);
        plSkinComponent* ptSkinComponent = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptScene->sbtSkinData[i].tEntity);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(plMat4) * pl_sb_size(ptSkinComponent->sbtTextureData));
    }

    pl_refr_perform_skinning(ptSkinningCmdBuffer, uSceneHandle);
    gptGfx->end_command_recording(ptSkinningCmdBuffer);

    const plSubmitInfo tSkinningSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
        .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
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
    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    const uint32_t uLightCount = pl_sb_size(sbtLights);
    for(uint32_t i = 0; i < uLightCount; i++)
    {
        const plLightComponent* ptLight = &sbtLights[i];

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
    pl_refr_pack_shadow_atlas(uSceneHandle, auViewHandles, uViewCount);
    pl_sb_reset(ptScene->sbtLightShadowData);

    const plBeginCommandInfo tShadowBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
        .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
    };

    plCommandBuffer* ptShadowCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptShadowCmdBuffer, &tShadowBeginInfo);

    plRenderEncoder* ptShadowEncoder = gptGfx->begin_render_pass(ptShadowCmdBuffer, ptScene->tFirstShadowRenderPass, NULL);

    pl_refr_generate_shadow_maps(ptShadowEncoder, ptShadowCmdBuffer, uSceneHandle);

    gptGfx->end_render_pass(ptShadowEncoder);
    gptGfx->end_command_recording(ptShadowCmdBuffer);

    const plSubmitInfo tShadowSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
        .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
    };
    gptGfx->submit_command_buffer(ptShadowCmdBuffer, &tShadowSubmitInfo);
    gptGfx->return_command_buffer(ptShadowCmdBuffer);

    plBuffer* ptShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptScene->atLightShadowDataBuffer[uFrameIdx]);
    memcpy(ptShadowDataBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptScene->sbtLightShadowData));


    // render views
    for(uint32_t uViewIndex = 0; uViewIndex < uViewCount; uViewIndex++)
    {
        ptScene->uDShadowIndex = 0;
        ptScene->uDShadowOffset = 0;

        const uint32_t uViewHandle = auViewHandles[uViewIndex];
        plRefView* ptView = &ptScene->atViews[uViewHandle];
        plCameraComponent* ptCamera = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *atOptions[uViewIndex].ptViewCamera);
        plCameraComponent* ptCullCamera = atOptions[uViewIndex].ptCullCamera ? gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *atOptions[uViewIndex].ptCullCamera) : ptCamera;
    
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate CSMs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        const plBeginCommandInfo tCSMBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
        };

        plCommandBuffer* ptCSMCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCSMCmdBuffer, &tCSMBeginInfo);

        pl_sb_reset(ptView->tDirectionLightShadowData.sbtDLightShadowData);
        ptView->tDirectionLightShadowData.uOffset = 0;
        ptView->tDirectionLightShadowData.uOffsetIndex = 0;

        plRenderEncoder* ptCSMEncoder = gptGfx->begin_render_pass(ptCSMCmdBuffer, ptScene->tShadowRenderPass, NULL);

        plCameraComponent* ptSceneCamera = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *atOptions[uViewIndex].ptViewCamera);
        pl_refr_generate_cascaded_shadow_map(ptCSMEncoder, ptCSMCmdBuffer, uSceneHandle, uViewHandle, 0, 0, &ptView->tDirectionLightShadowData,  ptSceneCamera);

        gptGfx->end_render_pass(ptCSMEncoder);
        gptGfx->end_command_recording(ptCSMCmdBuffer);

        const plSubmitInfo tCSMSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
        };
        gptGfx->submit_command_buffer(ptCSMCmdBuffer, &tCSMSubmitInfo);
        gptGfx->return_command_buffer(ptCSMCmdBuffer);

        plBuffer* ptDShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptView->tDirectionLightShadowData.atDLightShadowDataBuffer[uFrameIdx]);
        memcpy(ptDShadowDataBuffer->tMemoryAllocation.pHostMapped, ptView->tDirectionLightShadowData.sbtDLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptView->tDirectionLightShadowData.sbtDLightShadowData));
    }

    const uint32_t uProbeCount = pl_sb_size(ptScene->sbtProbeData);
    for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
    {
        plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
        plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, ptProbe->tEntity);
        
        if(!((ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_REALTIME) || (ptProbeComp->tFlags & PL_ENVIRONMENT_PROBE_FLAGS_DIRTY)))
        {
            continue;
        }

        plTransformComponent* ptProbeTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptProbe->tEntity);

        // reset probe data
        pl_sb_reset(ptProbe->tDirectionLightShadowData.sbtDLightShadowData);
        ptProbe->tDirectionLightShadowData.uOffset = 0;
        ptProbe->tDirectionLightShadowData.uOffsetIndex = 0;

        plCameraComponent atEnvironmentCamera[6] = {0};

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

            atEnvironmentCamera[uFace] = (plCameraComponent){
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
                .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
                .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
            };

            plCommandBuffer* ptCSMCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCSMCommandBuffer, &tBeginCSMInfo);

            plRenderEncoder* ptCSMEncoder = gptGfx->begin_render_pass(ptCSMCommandBuffer, ptScene->tShadowRenderPass, NULL);

            pl_refr_generate_cascaded_shadow_map(ptCSMEncoder, ptCSMCommandBuffer, uSceneHandle, uFace, uProbeIndex, 1, &ptProbe->tDirectionLightShadowData,  &atEnvironmentCamera[uFace]);

            gptGfx->end_render_pass(ptCSMEncoder);
            gptGfx->end_command_recording(ptCSMCommandBuffer);

            const plSubmitInfo tSubmitCSMInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
                .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
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
        plEnvironmentProbeComponent* ptProbe = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, ptScene->sbtProbeData[i].tEntity);
        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtProbeData[i].tEntity);
        plTransformComponent* ptProbeTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptScene->sbtProbeData[i].tEntity);
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
    gptData->aulNextTimelineValue[uFrameIdx] = pl__update_environment_probes(uSceneHandle, gptData->aulNextTimelineValue[uFrameIdx]);

    // common
    const plBindGroupUpdateSamplerData tSkyboxSamplerData = {
        .tSampler = gptData->tSkyboxSampler,
        .uSlot    = 1
    };

    const plBindGroupDesc tSkyboxBG0Desc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptData->tSkyboxBG0Layout,
        .pcDebugName = "skybox view specific bindgroup"
    };

    const plBindGroupDesc tDeferredBG1Desc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptData->tDeferredBG1Layout,
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
        .tLayout     = gptData->tPickBG0Layout,
        .pcDebugName = "temp pick bind group"
    };

    const plBindGroupDesc tJFABindGroupDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIdx],
        .tLayout     = gptData->tJFABGLayout,
        .pcDebugName = "temp jfa bind group"
    };

    for(uint32_t uViewIndex = 0; uViewIndex < uViewCount; uViewIndex++)
    {
        const uint32_t uViewHandle = auViewHandles[uViewIndex];
        plRefView* ptView = &ptScene->atViews[uViewHandle];
        plCameraComponent* ptCamera = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *atOptions[uViewIndex].ptViewCamera);
        plCameraComponent* ptCullCamera = atOptions[uViewIndex].ptCullCamera ? gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *atOptions[uViewIndex].ptCullCamera) : ptCamera;
    
        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

        if(!gptData->tRuntimeOptions.bFrustumCulling)
            ptCullCamera = NULL;

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
                .task  = pl__refr_cull_job,
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
            pl_sb_reset(ptView->sbtVisibleOpaqueDrawables);
            pl_sb_reset(ptView->sbtVisibleTransparentDrawables);
            pl_sb_reset(ptView->sbtVisibleDrawables);

            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                {
                    if(tDrawable.tFlags & PL_DRAWABLE_FLAG_DEFERRED)
                    {
                        pl_sb_push(ptView->sbtVisibleOpaqueDrawables, uDrawableIndex);
                        pl_sb_push(ptView->sbtVisibleDrawables, uDrawableIndex);
                    }
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_PROBE)
                    {
                        if(gptData->tRuntimeOptions.bShowProbes)
                        {
                            pl_sb_push(ptView->sbtVisibleTransparentDrawables, uDrawableIndex);
                            pl_sb_push(ptView->sbtVisibleDrawables, uDrawableIndex);
                        }
                    }
                    else if(tDrawable.tFlags & PL_DRAWABLE_FLAG_FORWARD)
                    {
                        pl_sb_push(ptView->sbtVisibleTransparentDrawables, uDrawableIndex);
                        pl_sb_push(ptView->sbtVisibleDrawables, uDrawableIndex);
                    }
                    
                }
            }
        }

        *gptData->pdDrawCalls += (double)(pl_sb_size(ptView->sbtVisibleOpaqueDrawables) + pl_sb_size(ptView->sbtVisibleTransparentDrawables) + 1);

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
            .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
        };

        plCommandBuffer* ptSceneCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptSceneCmdBuffer, &tSceneBeginInfo);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 0 - g buffer fill~~~~~~~~~~~~~~~~~~~~~~~~~~

        plRenderEncoder* ptSceneEncoder = gptGfx->begin_render_pass(ptSceneCmdBuffer, ptView->tRenderPass, NULL);
        gptGfx->set_depth_bias(ptSceneEncoder, 0.0f, 0.0f, 0.0f);

        const uint32_t uVisibleDeferredDrawCount = pl_sb_size(ptView->sbtVisibleOpaqueDrawables);
        gptGfx->reset_draw_stream(ptStream, uVisibleDeferredDrawCount);
        for(uint32_t i = 0; i < uVisibleDeferredDrawCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbtVisibleOpaqueDrawables[i]];

            if(tDrawable.uInstanceCount != 0)
            {
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
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

        typedef struct _plLightingDynamicData {
            uint32_t uGlobalIndex;
            uint32_t uUnused[3];
        } plLightingDynamicData;

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

        if(ptScene->tSkyboxTexture.uIndex != 0 && ptScene->tRuntimeOptions.bShowSkybox)
        {

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
                .tShader        = gptData->tSkyboxShader,
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
        
        const uint32_t uVisibleForwardDrawCount = pl_sb_size(ptView->sbtVisibleTransparentDrawables);
        gptGfx->reset_draw_stream(ptStream, uVisibleForwardDrawCount);
        for(uint32_t i = 0; i < uVisibleForwardDrawCount; i++)
        {
            const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbtVisibleTransparentDrawables[i]];

            if(tDrawable.uInstanceCount != 0)
            {
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
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
        const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlineDrawables);
        if(uOutlineDrawableCount > 0 && gptData->tRuntimeOptions.bShowSelectedBoundingBox)
        {
            const plVec4 tOutlineColor = (plVec4){0.0f, (float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 1.0f};
            for(uint32_t i = 0; i < uOutlineDrawableCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtOutlineDrawables[i];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                gptDraw->add_3d_aabb(ptView->pt3DSelectionDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tOutlineColor), .fThickness = 0.01f});
                
            }
        }

        // light drawing
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

        // debug drawing
        if(gptData->tRuntimeOptions.bDrawAllBoundingBoxes)
        {
            for(uint32_t i = 0; i < uDrawableCount; i++)
            {
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
        }
        else if(gptData->tRuntimeOptions.bDrawVisibleBoundingBoxes)
        {
            for(uint32_t i = 0; i < uVisibleDeferredDrawCount; i++)
            {
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtDrawables[ptView->sbtVisibleDrawables[i]].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
        }

        if(gptData->tRuntimeOptions.bShowOrigin)
        {
            const plMat4 tTransform = pl_identity_mat4();
            gptDraw->add_3d_transform(ptView->pt3DDrawList, &tTransform, 10.0f, (plDrawLineOptions){.fThickness = 0.02f});
        }

        if(gptData->tRuntimeOptions.bShowProbes)
        {
            for(uint32_t uProbeIndex = 0; uProbeIndex < uProbeCount; uProbeIndex++)
            {
                plEnvironmentProbeData* ptProbe = &ptScene->sbtProbeData[uProbeIndex];
                plEnvironmentProbeComponent* ptProbeComp = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_ENVIRONMENT_PROBE, ptProbe->tEntity);
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptProbe->tEntity);
                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.0f, 1.0f, 0.0f), .fThickness = 0.02f});
            }
        }

        if(gptData->tRuntimeOptions.bShowBVH)
        {
            pl_begin_cpu_sample(gptProfile, 0, "draw BVH");
            plObjectComponent* sbtComponents = ptScene->tComponentLibrary.tObjectComponentManager.pComponents;

            plBVHNode* ptNode = NULL;
            uint32_t uLeafIndex = UINT32_MAX;
            while (gptBvh->traverse(&ptScene->tBvh, &ptNode, &uLeafIndex))
            {
                if(uLeafIndex != UINT32_MAX)
                {
                    plObjectComponent* ptObject = &sbtComponents[uLeafIndex];
                    gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptObject->tAABB.tMin, ptObject->tAABB.tMax, (plDrawLineOptions){.fThickness = 0.02f, .uColor = PL_COLOR_32_DARK_BLUE});
                }
                else
                {
                    gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptNode->tAABB.tMin, ptNode->tAABB.tMax, (plDrawLineOptions){.fThickness = 0.02f, .uColor = PL_COLOR_32_WHITE});
                }
            }
            pl_end_cpu_sample(gptProfile, 0);
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

            typedef struct _plPickDynamicData
            {
                uint32_t uID;
                uint32_t _unused[3];
                plVec4 tMousePos;
                plMat4 tModel;
                
            } plPickDynamicData;

            plRenderEncoder* ptPickEncoder = gptGfx->begin_render_pass(ptSceneCmdBuffer, ptView->tPickRenderPass, NULL);
            gptGfx->set_depth_bias(ptPickEncoder, 0.0f, 0.0f, 0.0f);
            gptGfx->bind_shader(ptPickEncoder, gptData->tPickShader);
            gptGfx->bind_vertex_buffer(ptPickEncoder, ptScene->tVertexBuffer);

            plBindGroupHandle atBindGroups[2] = {tPickBG0, ptView->atPickBindGroup[uFrameIdx]};
            gptGfx->bind_graphics_bind_groups(ptPickEncoder, gptData->tPickShader, 0, 2, atBindGroups, 0, NULL);

            const uint32_t uVisibleDrawCount = pl_sb_size(ptView->sbtVisibleDrawables);
            *gptData->pdDrawCalls += (double)uVisibleDrawCount;

            plVec2 tMousePos = gptIOI->get_mouse_pos();
            tMousePos = pl_sub_vec2(tMousePos, ptView->tHoverOffset);
            tMousePos = pl_div_vec2(tMousePos, ptView->tHoverWindowRatio);
            
            for(uint32_t i = 0; i < uVisibleDrawCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtDrawables[ptView->sbtVisibleDrawables[i]];

                uint32_t uId = tDrawable.tEntity.uIndex;
                
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
                plPickDynamicData* ptDynamicData = (plPickDynamicData*)tDynamicBinding.pcData;
                
                ptDynamicData->uID = uId;
                ptDynamicData->tModel = ptTransform->tWorld;
                ptDynamicData->tMousePos.xy = tMousePos;
                
                gptGfx->bind_graphics_bind_groups(ptPickEncoder, gptData->tPickShader, 0, 0, NULL, 1, &tDynamicBinding);

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
            .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
        };
        gptGfx->submit_command_buffer(ptSceneCmdBuffer, &tSceneSubmitInfo);
        gptGfx->return_command_buffer(ptSceneCmdBuffer);


        plCommandBuffer* ptPickingDecodeCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);

        const plBeginCommandInfo tPickingDecodeBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
        };
        gptGfx->begin_command_recording(ptPickingDecodeCmdBuffer, &tPickingDecodeBeginInfo);

        gptGfx->end_command_recording(ptPickingDecodeCmdBuffer);

        const plSubmitInfo tPickingDecodeSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
        };
        gptGfx->submit_command_buffer(ptPickingDecodeCmdBuffer, &tPickingDecodeSubmitInfo);
        gptGfx->return_command_buffer(ptPickingDecodeCmdBuffer);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~uv map pass for JFA~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        const plBeginCommandInfo tUVBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
        };

        plCommandBuffer* ptUVCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptUVCmdBuffer, &tUVBeginInfo);

        plRenderEncoder* ptUVEncoder = gptGfx->begin_render_pass(ptUVCmdBuffer, ptView->tUVRenderPass, NULL);

        // submit nonindexed draw using basic API
        gptGfx->bind_shader(ptUVEncoder, gptData->tUVShader);
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
            .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
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

        for(uint32_t i = 0; i < uJumpSteps; i++)
        {
            const plBeginCommandInfo tJumpBeginInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
                .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
            };

            plCommandBuffer* ptJumpCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptJumpCmdBuffer, &tJumpBeginInfo);

            // begin main renderpass (directly to swapchain)
            plComputeEncoder* ptJumpEncoder = gptGfx->begin_compute_pass(ptJumpCmdBuffer, NULL);
            gptGfx->pipeline_barrier_compute(ptJumpEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_READ, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE);

            ptView->tLastUVMask = (i % 2 == 0) ? ptView->atUVMaskTexture1 : ptView->atUVMaskTexture0;

            // submit nonindexed draw using basic API
            gptGfx->bind_compute_shader(ptJumpEncoder, gptData->tJFAShader);

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            plVec4* ptJumpDistance = (plVec4*)tDynamicBinding.pcData;
            ptJumpDistance->x = tDimensions.x;
            ptJumpDistance->y = tDimensions.y;
            ptJumpDistance->z = fJumpDistance;

            gptGfx->bind_compute_bind_groups(ptJumpEncoder, gptData->tJFAShader, 0, 1, &atJFABindGroups[i % 2], 1, &tDynamicBinding);
            gptGfx->dispatch(ptJumpEncoder, 1, &tDispach);

            // end render pass
            gptGfx->pipeline_barrier_compute(ptJumpEncoder, PL_PIPELINE_STAGE_COMPUTE_SHADER, PL_ACCESS_SHADER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_FRAGMENT_SHADER, PL_ACCESS_SHADER_READ);
            gptGfx->end_compute_pass(ptJumpEncoder);

            // end recording
            gptGfx->end_command_recording(ptJumpCmdBuffer);

            const plSubmitInfo tJumpSubmitInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
                .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]},
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
            .atWaitSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auWaitSemaphoreValues = {gptData->aulNextTimelineValue[uFrameIdx]},
        };

        plCommandBuffer* ptPostCmdBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptPostCmdBuffer, &tPostBeginInfo);

        pl_refr_post_process_scene(ptPostCmdBuffer, uSceneHandle, uViewHandle, &tMVP);
        gptGfx->end_command_recording(ptPostCmdBuffer);

        const plSubmitInfo tPostSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
            .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]}
        };
        gptGfx->submit_command_buffer(ptPostCmdBuffer, &tPostSubmitInfo);
        gptGfx->return_command_buffer(ptPostCmdBuffer);

        // update stats
        static double* pdVisibleOpaqueObjects = NULL;
        static double* pdVisibleTransparentObjects = NULL;
        if(!pdVisibleOpaqueObjects)
        {
            pdVisibleOpaqueObjects = gptStats->get_counter("visible opaque objects");
            pdVisibleTransparentObjects = gptStats->get_counter("visible transparent objects");
        }

        // only record stats for first scene
        if(uSceneHandle == 0)
        {
            *pdVisibleOpaqueObjects = (double)(pl_sb_size(ptView->sbtVisibleOpaqueDrawables));
            *pdVisibleTransparentObjects = (double)(pl_sb_size(ptView->sbtVisibleTransparentDrawables));
        }

    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_resize(void)
{
    gptData->tRuntimeOptions.bReloadMSAA = true;

    pl_log_info(gptLog, gptData->uLogChannel, "resizing");

    plSwapchainInit tDesc = {
        .bVSync  = gptData->tRuntimeOptions.bVSync,
        .uWidth  = (uint32_t)(gptIO->tMainViewportSize.x * gptIO->tMainFramebufferScale.x),
        .uHeight = (uint32_t)(gptIO->tMainViewportSize.y * gptIO->tMainFramebufferScale.y),
        .tSampleCount = gptData->tRuntimeOptions.bMSAA ? gptData->tDeviceInfo.tMaxSampleCount : PL_SAMPLE_COUNT_1
    };
    gptGfx->recreate_swapchain(gptData->ptSwap, &tDesc);
}

static bool
pl_refr_begin_frame(void)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    plDevice* ptDevice = gptData->ptDevice;
    gptGfx->begin_frame(ptDevice);

    if(gptData->tRuntimeOptions.bReloadSwapchain)
    {
        gptData->tRuntimeOptions.bReloadSwapchain = false;
        pl_refr_resize();
        pl_end_cpu_sample(gptProfile, 0);
        return false;
    }

    if(gptData->tRuntimeOptions.bReloadMSAA)
    {
        gptData->tRuntimeOptions.bReloadMSAA = false;

        uint32_t uImageCount = 0;
        plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptData->ptSwap, &uImageCount);

        if(gptData->tRuntimeOptions.bMSAA)
        {
            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptData->atCmdPools[0]);
            gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    
            // begin blit pass, copy buffer, end pass
            plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
            gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    
            plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptData->ptSwap);
            const plTextureDesc tColorTextureDesc = {
                .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
                .tFormat       = tInfo.tFormat,
                .uLayers       = 1,
                .uMips         = 1,
                .tType         = PL_TEXTURE_TYPE_2D,
                .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .pcDebugName   = "MSAA color texture",
                .tSampleCount  = tInfo.tSampleCount
            };
    
            gptGfx->queue_texture_for_deletion(gptData->ptDevice, gptData->tMSAATexture);
    
            // create textures
            gptData->tMSAATexture = gptGfx->create_texture(gptData->ptDevice, &tColorTextureDesc, NULL);
    
            // retrieve textures
            plTexture* ptColorTexture = gptGfx->get_texture(gptData->ptDevice, gptData->tMSAATexture);
    
            // allocate memory
            plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
            if(ptColorTexture->tMemoryRequirements.ulSize > gptGpuAllocators->get_buddy_block_size())
                ptAllocator = gptData->ptLocalDedicatedAllocator;
            const plDeviceMemoryAllocation tColorAllocation = ptAllocator->allocate(ptAllocator->ptInst, 
                ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
                ptColorTexture->tMemoryRequirements.ulSize,
                ptColorTexture->tMemoryRequirements.ulAlignment,
                "MSAA texture memory");
    
            // bind memory
            gptGfx->bind_texture_to_memory(gptData->ptDevice, gptData->tMSAATexture, &tColorAllocation);
    
            // set initial usage
            gptGfx->set_texture_usage(ptEncoder, gptData->tMSAATexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);
    
            gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
            gptGfx->end_blit_pass(ptEncoder);
    
            // finish recording
            gptGfx->end_command_recording(ptCommandBuffer);
    
            // submit command buffer
            gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
            gptGfx->wait_on_command_buffer(ptCommandBuffer);
            gptGfx->return_command_buffer(ptCommandBuffer);

            plRenderPassAttachments atMainMSAAAttachmentSets[16] = {0};
            for(uint32_t i = 0; i < uImageCount; i++)
            {
                atMainMSAAAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
                atMainMSAAAttachmentSets[i].atViewAttachments[1] = gptData->tMSAATexture;
            }

            if(gptData->tMainMSAARenderPass.uIndex == 0)
            {
                const plRenderPassDesc tMainMSAARenderPassDesc = {
                    .tLayout = gptData->tMainMSAARenderPassLayout,
                    .tResolveTarget = { // swapchain image
                        .tLoadOp       = PL_LOAD_OP_DONT_CARE,
                        .tStoreOp      = PL_STORE_OP_STORE,
                        .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                        .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                        .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
                    },
                    .atColorTargets = { // msaa
                        {
                            .tLoadOp       = PL_LOAD_OP_CLEAR,
                            .tStoreOp      = PL_STORE_OP_STORE_MULTISAMPLE_RESOLVE,
                            .tCurrentUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                            .tNextUsage    = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                            .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
                        }
                    },
                    .tDimensions = {(float)tInfo.uWidth, (float)tInfo.uHeight},
                    .ptSwapchain = gptData->ptSwap
                };
                gptData->tMainMSAARenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tMainMSAARenderPassDesc, atMainMSAAAttachmentSets);
            }
            else
                gptGfx->update_render_pass_attachments(gptData->ptDevice, gptData->tMainMSAARenderPass, tColorTextureDesc.tDimensions.xy, atMainMSAAAttachmentSets);
            
            gptData->tCurrentMainRenderPass = gptData->tMainMSAARenderPass;
        }
        else
        {


            plRenderPassAttachments atMainAttachmentSets[16] = {0};
            for(uint32_t i = 0; i < uImageCount; i++)
            {
                atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
            }
            plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptData->ptSwap);
            const plRenderPassDesc tMainRenderPassDesc = {
                .tLayout = gptData->tMainRenderPassLayout,
                .atColorTargets = { // msaa
                    {
                        .tLoadOp       = PL_LOAD_OP_CLEAR,
                        .tStoreOp      = PL_STORE_OP_STORE,
                        .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                        .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                        .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
                    }
                },
                .tDimensions = {(float)tInfo.uWidth, (float)tInfo.uHeight},
                .ptSwapchain = gptData->ptSwap
            };
            if(gptData->tMainRenderPass.uIndex == 0)
            {

                gptData->tMainRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);
            }
            else
                gptGfx->update_render_pass_attachments(gptData->ptDevice, gptData->tMainRenderPass, tMainRenderPassDesc.tDimensions, atMainAttachmentSets);
            
            gptData->tCurrentMainRenderPass = gptData->tMainRenderPass;
        }

    }

    gptGfx->reset_command_pool(gptData->atCmdPools[gptGfx->get_current_frame_index()], 0);
    gptGfx->reset_bind_group_pool(gptData->aptTempGroupPools[gptGfx->get_current_frame_index()]);
    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);

    if(!gptGfx->acquire_swapchain_image(gptData->ptSwap))
    {
        pl_refr_resize();
        pl_end_cpu_sample(gptProfile, 0);
        return false;
    }

    // perform GPU buffer updates
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptData->atCmdPools[uFrameIdx];
    uint64_t ulValue = gptData->aulNextTimelineValue[uFrameIdx];
    plTimelineSemaphore* tSemHandle = gptData->aptSemaphores[uFrameIdx];

    for(uint32_t uSceneIndex = 0; uSceneIndex < pl_sb_size(gptData->sbtScenes); uSceneIndex++)
    {
        plRefScene* ptScene = &gptData->sbtScenes[uSceneIndex];
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
    
                gptData->atStagingBufferHandle[uFrameIdx].tStagingBufferHandle = pl__refr_create_staging_buffer(&tStagingBufferDesc, "staging", uFrameIdx);
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

        for(uint32_t uViewIndex = 0; uViewIndex < ptScene->uViewCount; uViewIndex++)
        {
            plRefView* ptView = &ptScene->atViews[uViewIndex];

            if(ptView->auHoverResultProcessing[uFrameIdx])
            {
                pl_begin_cpu_sample(gptProfile, 0, "Picking Retrieval");
                
                plBuffer* ptPickBuffer = gptGfx->get_buffer(ptDevice, ptView->atPickBuffer[uFrameIdx]);
                const uint32_t uNewID = *(uint32_t*)ptPickBuffer->tMemoryAllocation.pHostMapped;
                plEntity tNewEntity = {
                    .uIndex = uNewID,
                    .uGeneration = ptScene->tComponentLibrary.sbtEntityGenerations[uNewID]
                };
                ptView->tHoveredEntity = tNewEntity;

                ptView->auHoverResultProcessing[uFrameIdx] = false;
                ptView->auHoverResultReady[uFrameIdx] = true;
                memset(ptPickBuffer->tMemoryAllocation.pHostMapped, 0, sizeof(uint32_t) * 2);
        
                pl_end_cpu_sample(gptProfile, 0);
            }
        }
    }

    gptData->aulNextTimelineValue[uFrameIdx] = ulValue;
    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

static plRenderPassHandle
pl_refr_get_main_render_pass(void)
{
    return gptData->tCurrentMainRenderPass;
}

static void
pl_refr_begin_final_pass(plRenderEncoder** pptEncoder, plCommandBuffer** pptCommandBuffer)
{
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    plDevice*   ptDevice   = gptData->ptDevice;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    uint64_t ulValue = gptData->aulNextTimelineValue[uFrameIdx];
    plTimelineSemaphore* tSemHandle = gptData->aptSemaphores[uFrameIdx];

    // final work set
    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {tSemHandle},
        .auWaitSemaphoreValues = {ulValue},
    };
    
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, gptData->tCurrentMainRenderPass, NULL);

    *pptEncoder = ptEncoder;
    *pptCommandBuffer = ptCommandBuffer;
}

static void
pl_refr_end_final_pass(plRenderEncoder* ptEncoder, plCommandBuffer* ptCommandBuffer)
{
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    gptGfx->end_render_pass(ptEncoder);

    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptData->aptSemaphores[uFrameIdx]},
        .auSignalSemaphoreValues = {++gptData->aulNextTimelineValue[uFrameIdx]},
    };
    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, &gptData->ptSwap, 1))
    {
        pl_refr_resize();
    }

    gptGfx->return_command_buffer(ptCommandBuffer);
}

static plDrawList3D*
pl_refr_get_debug_drawlist(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    return ptView->pt3DDrawList;
}

static plDrawList3D*
pl_refr_get_gizmo_drawlist(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    return ptView->pt3DGizmoDrawList;
}

static plBindGroupHandle
pl_refr_get_view_color_texture(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->atViews[uViewHandle];
    return ptView->tFinalTextureHandle;
}

static void
pl_add_drawable_objects_to_scene(uint32_t uSceneHandle, uint32_t uObjectCount, const plEntity* atObjects)
{

    if(uObjectCount == 0)
        return;

    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    uint32_t uStart = pl_sb_size(ptScene->sbtStagedDrawables);
    pl_sb_add_n(ptScene->sbtStagedDrawables, uObjectCount);

    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        ptScene->sbtStagedDrawables[uStart + i].tEntity = atObjects[i];
        
        plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, atObjects[i]);
        plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        bool bForward = false;

        if(ptMaterial->tBaseColor.a != 1.0f || ptMaterial->tEmissiveColor.a > 0.0f)
            bForward = true;

        if(ptMaterial->tBlendMode == PL_BLEND_MODE_ALPHA)
            bForward = true;

        if(gptResource->is_valid(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].tResource))
            bForward = true;

        if(bForward)
            ptScene->sbtStagedDrawables[uStart + i].tFlags = PL_DRAWABLE_FLAG_FORWARD;
        else
            ptScene->sbtStagedDrawables[uStart + i].tFlags = PL_DRAWABLE_FLAG_DEFERRED;
        
    }

    // sort to group entities for instances (slow bubble sort, improve later)
    bool bSwapped = false;
    for (uint32_t i = 0; i < uObjectCount - 1; i++)
    {
        bSwapped = false;
        for (uint32_t j = 0; j < uObjectCount - i - 1; j++)
        {
            plObjectComponent* ptObjectA = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtStagedDrawables[uStart + j].tEntity);
            plObjectComponent* ptObjectB = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtStagedDrawables[uStart + j + 1].tEntity);
            if (ptObjectA->tMesh.uIndex > ptObjectB->tMesh.uIndex)
            {
                plEntity tA = ptScene->sbtStagedDrawables[uStart + j].tEntity;
                plEntity tB = ptScene->sbtStagedDrawables[uStart + j + 1].tEntity;
                ptScene->sbtStagedDrawables[uStart + j].tEntity = tB;
                ptScene->sbtStagedDrawables[uStart + j + 1].tEntity = tA;
                bSwapped = true;
            }
        }
      
        // If no two elements were swapped, then break
        if (!bSwapped)
            break;
    }

    for (uint32_t i = 0; i < uObjectCount; i++)
    {
        ptScene->sbtStagedDrawables[uStart + i].uInstanceCount = 1;
        ptScene->sbtStagedDrawables[uStart + i].uTransformIndex = ptScene->uNextTransformIndex++;
        plObjectComponent* ptObjectA = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtStagedDrawables[uStart + i].tEntity);
        for (uint32_t j = i; j < uObjectCount - 1; j++)
        {
            plObjectComponent* ptObjectB = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, ptScene->sbtStagedDrawables[uStart + j + 1].tEntity);
            if(ptObjectA->tMesh.uIndex == ptObjectB->tMesh.uIndex)
            {
                ptScene->sbtStagedDrawables[uStart + i].uInstanceCount++;

                ptScene->sbtStagedDrawables[uStart + j + 1].uInstanceCount = 0;
                ptScene->sbtStagedDrawables[uStart + j + 1].uTransformIndex = ptScene->uNextTransformIndex++;
            }
            else
            {
                break;
            }
            
        }
        i += ptScene->sbtStagedDrawables[uStart + i].uInstanceCount;
        i--;

    }

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_update_scene_materials(uint32_t uSceneHandle, uint32_t uMaterialCount, const plEntity* atMaterials)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        const plEntity tMaterialComp = atMaterials[i];
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, tMaterialComp);

        uint32_t uMaterialIndex = (uint32_t)pl_hm_lookup(&ptScene->tMaterialHashmap, tMaterialComp.ulData);
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

            int iBaseColorTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tBaseColorTex);
            int iNormalTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tNormalTex);
            int iEmissiveTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tEmissiveTex);
            int iMetallicRoughnessTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tMetallicRoughnessTex);
            int iOcclusionTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tOcclusionTex);

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

static void
pl_refr_remove_objects_from_scene(uint32_t uSceneHandle, uint32_t uObjectCount, const plEntity* atObjects)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        const plEntity tObject = atObjects[i];

        uint32_t uDrawableCount = pl_sb_size(ptScene->sbtStagedDrawables);
        for(uint32_t j = 0; j < uDrawableCount; j++)
        {
            if(ptScene->sbtStagedDrawables[j].tEntity.ulData == tObject.ulData)
            {
                pl_hm_remove(&ptScene->tDrawableHashmap, tObject.ulData);
                pl_sb_del_swap(ptScene->sbtStagedDrawables, j);
                j--;
                uDrawableCount--;
            }
        }
    }

    pl__refr_unstage_drawables(uSceneHandle);
    pl__refr_set_drawable_shaders(uSceneHandle);
    pl__refr_sort_drawables(uSceneHandle);
    
    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_update_scene_objects(uint32_t uSceneHandle, uint32_t uObjectCount, const plEntity* atObjects)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    pl__refr_sort_drawables(uSceneHandle);
    
    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_add_materials_to_scene(uint32_t uSceneHandle, uint32_t uMaterialCount, const plEntity* atMaterials)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    pl_sb_reset(ptScene->sbtMaterialBuffer);

    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        const plEntity tMaterialComp = atMaterials[i];
        plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, tMaterialComp);

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
        if(!pl_hm_has_key_ex(&ptScene->tMaterialHashmap, tMaterialComp.ulData, &uMaterialIndex))
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

            int iBaseColorTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tBaseColorTex);
            int iNormalTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tNormalTex);
            int iEmissiveTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tEmissiveTex);
            int iMetallicRoughnessTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tMetallicRoughnessTex);
            int iOcclusionTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tOcclusionTex);

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
            pl_hm_insert(&ptScene->tMaterialHashmap, tMaterialComp.ulData, ulValue);
        }
    }
    pl_end_cpu_sample(gptProfile, 0);
}

plRendererRuntimeOptions*
pl_refr_get_runtime_options(void)
{
    return &gptData->tRuntimeOptions;
}

plSceneRuntimeOptions*
pl_refr_get_scene_runtime_options(uint32_t uSceneHandle)
{
    return &gptData->sbtScenes[uSceneHandle].tRuntimeOptions;
}

void
pl_refr_rebuild_bvh(uint32_t uSceneHandle)
{
    plComponentLibrary* ptLibrary = &gptData->sbtScenes[uSceneHandle].tComponentLibrary;
    plObjectComponent* sbtComponents = ptLibrary->tObjectComponentManager.pComponents;
    const uint32_t uObjectCount = pl_sb_size(sbtComponents);
    pl_sb_resize(gptData->sbtScenes[uSceneHandle].sbtBvhAABBs, uObjectCount);
    for(uint32_t j = 0; j < uObjectCount; j++)
    {
        plObjectComponent* ptObject = &sbtComponents[j];
        gptData->sbtScenes[uSceneHandle].sbtBvhAABBs[j] = ptObject->tAABB;
    }

    gptBvh->build(&gptData->sbtScenes[uSceneHandle].tBvh, gptData->sbtScenes[uSceneHandle].sbtBvhAABBs, uObjectCount);

    pl_sb_reset(gptData->sbtScenes[uSceneHandle].sbtBvhAABBs); 
}

static plCommandPool*
pl__refr_get_command_pool(void)
{
    plCommandPool* ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    return ptCmdPool;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_renderer_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plRendererI tApi = {
        .initialize                         = pl_refr_initialize,
        .cleanup                            = pl_refr_cleanup,
        .create_scene                       = pl_refr_create_scene,
        .cleanup_scene                      = pl_refr_cleanup_scene,
        .add_drawable_objects_to_scene      = pl_add_drawable_objects_to_scene,
        .add_materials_to_scene             = pl_refr_add_materials_to_scene,
        .remove_objects_from_scene          = pl_refr_remove_objects_from_scene,
        .update_scene_objects               = pl_refr_update_scene_objects,
        .update_scene_materials             = pl_refr_update_scene_materials,
        .create_view                        = pl_refr_create_view,
        .cleanup_view                       = pl_refr_cleanup_view,
        .run_ecs                            = pl_refr_run_ecs,
        .begin_frame                        = pl_refr_begin_frame,
        .begin_final_pass                   = pl_refr_begin_final_pass,
        .end_final_pass                     = pl_refr_end_final_pass,
        .get_component_library              = pl_refr_get_component_library,
        .get_device                         = pl_refr_get_device,
        .get_swapchain                      = pl_refr_get_swapchain,
        .load_skybox_from_panorama          = pl_refr_load_skybox_from_panorama,
        .finalize_scene                     = pl_refr_finalize_scene,
        .reload_scene_shaders               = pl_refr_reload_scene_shaders,
        .outline_entities                   = pl_refr_outline_entities,
        .render_scene                       = pl_refr_render_scene,
        .get_view_color_texture             = pl_refr_get_view_color_texture,
        .resize_view                        = pl_refr_resize_view,
        .get_debug_drawlist                 = pl_refr_get_debug_drawlist,
        .get_gizmo_drawlist                 = pl_refr_get_gizmo_drawlist,
        .update_hovered_entity              = pl_refr_update_hovered_entity,
        .get_hovered_entity                 = pl_refr_get_hovered_entity,
        .get_command_pool                   = pl__refr_get_command_pool,
        .resize                             = pl_refr_resize,
        .get_main_render_pass               = pl_refr_get_main_render_pass,
        .get_runtime_options                = pl_refr_get_runtime_options,
        .get_scene_runtime_options          = pl_refr_get_scene_runtime_options,
        .rebuild_scene_bvh                  = pl_refr_rebuild_bvh,
    };
    pl_set_api(ptApiRegistry, plRendererI, &tApi);

    // core apis
    gptDataRegistry  = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptIOI           = pl_get_api_latest(ptApiRegistry, plIOI);
    gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
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
    gptPhysics       = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptBvh           = pl_get_api_latest(ptApiRegistry, plBVHI);

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