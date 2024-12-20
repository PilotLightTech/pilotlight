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
static void     pl_refr_initialize(plWindow*);
static void     pl_refr_cleanup(void);

// scenes
static uint32_t pl_refr_create_scene(void);
static void     pl_add_drawable_objects_to_scene(uint32_t,  uint32_t, const plEntity*, uint32_t, const plEntity*);

// views
static uint32_t          pl_refr_create_view(uint32_t, plVec2);
static plBindGroupHandle pl_refr_get_view_color_texture(uint32_t, uint32_t);
static void              pl_refr_resize_view(uint32_t, uint32_t, plVec2);
static void              pl_refr_resize(void);

// loading
static void pl_refr_load_skybox_from_panorama(uint32_t, const char*, int);
static void pl_refr_finalize_scene(uint32_t);
static void pl_refr_reload_scene_shaders(uint32_t);

// ui
static void pl_show_graphics_options(const char*);

// per frame
static void     pl_refr_run_ecs(uint32_t uSceneHandle);
static void     pl_refr_render_scene(uint32_t, uint32_t, plViewOptions);
static bool     pl_refr_begin_frame(void);
static void     pl_refr_end_frame(void);
static plEntity pl_refr_get_picked_entity(void);

// misc.
static void                pl_refr_select_entities(uint32_t, uint32_t, plEntity*);
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
pl_refr_initialize(plWindow* ptWindow)
{

    // allocate renderer data
    gptData = PL_ALLOC(sizeof(plRefRendererData));
    memset(gptData, 0, sizeof(plRefRendererData));

    // register data with registry (for reloads)
    gptDataRegistry->set_data("ref renderer data", gptData);

    // add specific log channel for renderer
    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_BUFFER,
        .uEntryCount = 1024
    };
    gptData->uLogChannel = gptLog->add_channel("Renderer", tLogInit);

    // default options
    gptData->bVSync = true;
    gptData->uOutlineWidth = 4;
    gptData->fLambdaSplit = 0.95f;
    gptData->bFrustumCulling = true;

    // picking defaults
    gptData->uClickedFrame = UINT32_MAX;
    gptData->tPickedEntity.ulData = UINT64_MAX;

    // shader default values
    gptData->tSkyboxShader = (plShaderHandle){0}; // is this needed still?

    // initialize graphics
    plGraphicsInit tGraphicsDesc = {
        .tFlags = PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED
    };
    #ifndef NDEBUG
        tGraphicsDesc.tFlags |= PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;
    #endif
    gptGfx->initialize(&tGraphicsDesc);

    gptData->ptSurface = gptGfx->create_surface(ptWindow);

    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    gptGfx->enumerate_devices(atDeviceInfos, &uDeviceCount);

    // we will prefer discrete, then integrated GPUs
    int iBestDvcIdx = 0;
    int iDiscreteGPUIdx   = -1;
    int iIntegratedGPUIdx = -1;
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        
        if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_DISCRETE)
            iDiscreteGPUIdx = i;
        else if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_INTEGRATED)
            iIntegratedGPUIdx = i;
    }

    if(iDiscreteGPUIdx > -1)
        iBestDvcIdx = iDiscreteGPUIdx;
    else if(iIntegratedGPUIdx > -1)
        iBestDvcIdx = iIntegratedGPUIdx;

    // create device
    const plDeviceInit tDeviceInit = {
        .uDeviceIdx = iBestDvcIdx,
        .ptSurface = gptData->ptSurface,
        .szDynamicBufferBlockSize = 134217728
    };
    gptData->ptDevice = gptGfx->create_device(&tDeviceInit);

    gptData->tDeviceInfo = atDeviceInfos[iBestDvcIdx];
    if(gptData->tDeviceInfo.tCapabilities & PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS)
        gptData->bMultiViewportShadows = true;

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

    // create swapchain
    const plSwapchainInit tSwapInit = {
        .bVSync = true,
        .tSampleCount = atDeviceInfos[iBestDvcIdx].tMaxSampleCount
    };
    gptData->ptSwap = gptGfx->create_swapchain(gptData->ptDevice, gptData->ptSurface, &tSwapInit);
    gptDataRegistry->set_data("device", gptData->ptDevice); // used by debug extension

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
    gptData->ptLocalBuddyAllocator      = gptGpuAllocators->get_local_buddy_allocator(gptData->ptDevice);
    gptData->ptLocalDedicatedAllocator  = gptGpuAllocators->get_local_dedicated_allocator(gptData->ptDevice);
    gptData->ptStagingUnCachedAllocator = gptGpuAllocators->get_staging_uncached_allocator(gptData->ptDevice);
    gptData->ptStagingCachedAllocator   = gptGpuAllocators->get_staging_cached_allocator(gptData->ptDevice);

    // create staging buffers
    const plBufferDesc tStagingBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STAGING,
        .szByteSize = 268435456
    };
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptData->tStagingBufferHandle[i] = pl__refr_create_staging_buffer(&tStagingBufferDesc, "staging", i);

    // create caching staging buffer
    const plBufferDesc tStagingCachedBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_STAGING,
        .szByteSize = 268435456
    };
    gptData->tCachedStagingBuffer = pl__refr_create_cached_staging_buffer(&tStagingBufferDesc, "cached staging", 0);

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
        .pcDebugName     = "default sampler"
    };
    gptData->tDefaultSampler = gptGfx->create_sampler(gptData->ptDevice, &tSamplerDesc);

    const plSamplerDesc tShadowSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 1.0f,
        .fMaxAnisotropy  = 1.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tUAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tMipmapMode     = PL_MIPMAP_MODE_NEAREST,
        .pcDebugName     = "shadow sampler"
    };
    gptData->tShadowSampler = gptGfx->create_sampler(gptData->ptDevice, &tShadowSamplerDesc);

    const plSamplerDesc tEnvSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 64.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .tUAddressMode   = PL_ADDRESS_MODE_CLAMP,
        .pcDebugName     = "ENV sampler"
    };
    gptData->tEnvSampler = gptGfx->create_sampler(gptData->ptDevice, &tEnvSamplerDesc);

    // create deferred render pass layout
    const plRenderPassLayoutDesc tRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true },  // depth buffer
            { .tFormat = PL_FORMAT_R16G16B16A16_FLOAT }, // final output
            { .tFormat = PL_FORMAT_R8G8B8A8_SRGB },      // albedo
            { .tFormat = PL_FORMAT_R16G16_FLOAT }, // normal
            { .tFormat = PL_FORMAT_R16G16B16A16_FLOAT }, // AO, roughness, metallic, mip count
        },
        .atSubpasses = {
            { // G-buffer fill
                .uRenderTargetCount = 4,
                .auRenderTargets = {0, 2, 3, 4}
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
                .auSubpassInputs = {0}
            },
        }
    };
    gptData->tRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tRenderPassLayoutDesc);

    // create depth render pass layout
    const plRenderPassLayoutDesc tDepthRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT, .bDepth = true },  // depth buffer
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0},
            }
        }
    };
    gptData->tDepthRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tDepthRenderPassLayoutDesc);

    // create pick render pass layout
    const plRenderPassLayoutDesc tPickRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT, .bDepth = true },  // depth buffer
            { .tFormat = PL_FORMAT_R8G8B8A8_UNORM }, // final output
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
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
        }
    };
    gptData->tUVRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tUVRenderPassLayoutDesc);

    // create template shaders

    pl_refr_create_global_shaders();

    const plComputeShaderDesc tComputeShaderDesc = {
        .tShader = gptShader->load_glsl("../shaders/jumpfloodalgo.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 0, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                    {.uSlot = 1, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE}
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
        .szByteSize = sizeof(uint32_t) * 6
    };
    gptData->tFullQuadIndexBuffer = pl__refr_create_local_buffer(&tFullQuadIndexBufferDesc, "full quad index buffer", 0, auFullQuadIndexBuffer);

    const float afFullQuadVertexBuffer[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f
    };
    const plBufferDesc tFullQuadVertexBufferDesc = {
        .tUsage     = PL_BUFFER_USAGE_VERTEX,
        .szByteSize = sizeof(float) * 16
    };
    gptData->tFullQuadVertexBuffer = pl__refr_create_local_buffer(&tFullQuadVertexBufferDesc, "full quad vertex buffer", 0, afFullQuadVertexBuffer);

    // create semaphores
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptData->aptSemaphores[i] = gptGfx->create_semaphore(gptData->ptDevice, false);

    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat, .bResolve = true }, // swapchain
            { .tFormat = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat, .tSamples = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount}, // msaa
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
        }
    };
    gptData->tMainRenderPassLayout = gptGfx->create_render_pass_layout(gptData->ptDevice, &tMainRenderPassLayoutDesc);

    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptData->ptSwap);

    const plTextureDesc tColorTextureDesc = {
        .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
        .tFormat       = gptGfx->get_swapchain_info(gptData->ptSwap).tFormat,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
        .pcDebugName   = "offscreen color texture",
        .tSampleCount  = tInfo.tSampleCount
    };

    // create textures
    gptData->tMSAATexture = gptGfx->create_texture(gptData->ptDevice, &tColorTextureDesc, NULL);

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);


    // retrieve textures
    plTexture* ptColorTexture = gptGfx->get_texture(gptData->ptDevice, gptData->tMSAATexture);

    // allocate memory
    const plDeviceMemoryAllocation tColorAllocation = gptGfx->allocate_memory(gptData->ptDevice, 
        ptColorTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptColorTexture->tMemoryRequirements.uMemoryTypeBits,
        "color texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(gptData->ptDevice, gptData->tMSAATexture, &tColorAllocation);

    // set initial usage
    gptGfx->set_texture_usage(ptEncoder, gptData->tMSAATexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    const plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = gptData->tMainRenderPassLayout,
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
    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptData->ptSwap, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
        atMainAttachmentSets[i].atViewAttachments[1] = gptData->tMSAATexture;
    }
    gptData->tMainRenderPass = gptGfx->create_render_pass(gptData->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);
};

static uint32_t
pl_refr_create_scene(void)
{
    const uint32_t uSceneHandle = pl_sb_size(gptData->sbtScenes);
    plRefScene tScene = {0};
    pl_sb_push(gptData->sbtScenes, tScene);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // initialize ecs library
    gptECS->init_component_library(&ptScene->tComponentLibrary);

    // create global bindgroup
    ptScene->uTextureIndexCount = 0;

    plBindGroupLayout tGlobalBindGroupLayout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
            },
            .atSamplerBindings = {
                {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
            },
            .atTextureBindings = {
                {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                {.uSlot = 4100, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
            }
    };

    const plBindGroupDesc tGlobalBindGroupDesc = {
        .ptPool      = gptData->ptBindGroupPool,
        .ptLayout    = &tGlobalBindGroupLayout,
        .pcDebugName = "global bind group"
    };
    ptScene->tGlobalBindGroup = gptGfx->create_bind_group(gptData->ptDevice, &tGlobalBindGroupDesc);

    plBindGroupUpdateSamplerData tGlobalSamplerData[] = {
        {
            .tSampler = gptData->tDefaultSampler,
            .uSlot    = 2
        },
        {
            .tSampler = gptData->tEnvSampler,
            .uSlot    = 3
        }
    };

    plBindGroupUpdateData tGlobalBindGroupData = {
        .uSamplerCount = 2,
        .atSamplerBindings = tGlobalSamplerData,
    };

    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tGlobalBindGroup, &tGlobalBindGroupData);

    ptScene->uGGXLUT = pl__get_bindless_texture_index(uSceneHandle, gptData->tDummyTexture);
    ptScene->uLambertianEnvSampler = pl__get_bindless_cube_texture_index(uSceneHandle, gptData->tDummyTextureCube);
    ptScene->uGGXEnvSampler = pl__get_bindless_cube_texture_index(uSceneHandle, gptData->tDummyTextureCube);

    return uSceneHandle;
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
    ptView->tShadowData.tResolution.x = 1024.0f * 2.0f;
    ptView->tShadowData.tResolution.y = 1024.0f * 2.0f;

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

    const plTextureDesc tPickDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "pick depth original"
    };

    const plTextureDesc tNormalTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        // .tFormat       = PL_FORMAT_R16G16B16A16_FLOAT,
        .tFormat       = PL_FORMAT_R16G16_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_INPUT_ATTACHMENT,
        .pcDebugName   = "g-buffer normal"
    };

    const plTextureDesc tAlbedoTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_SRGB,
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

    const plTextureDesc tShadowDepthTextureDesc = {
        .tDimensions   = {ptView->tShadowData.tResolution.x, ptView->tShadowData.tResolution.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT,
        .uLayers       = 4,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D_ARRAY,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "shadow map"
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
        .szByteSize = 134217728,
        .pcDebugName = "global buffer"
    };

    const plBufferDesc atCameraBuffersDesc = {
        .tUsage     = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 4096,
        .pcDebugName = "camera buffers"
    };

    const plBufferDesc atLightShadowDataBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
        .szByteSize = 134217728,
        .pcDebugName = "shadow data buffer"
    };

    const plBindGroupLayout tLightingBindGroupLayout = {
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
        }
    };

    // create offscreen render pass
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atShadowAttachmentSets[PL_MAX_SHADOW_CASCADES][PL_MAX_FRAMES_IN_FLIGHT] = {0};

    ptView->tPickTexture = pl__refr_create_texture(&tPickTextureDesc, "pick original", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tPickDepthTexture = pl__refr_create_texture(&tPickDepthTextureDesc, "pick depth original", 0, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        // textures
        ptView->tFinalTexture[i]            = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen final", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tFinalTextureHandle[i]      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture[i]);
        ptView->tRawOutputTexture[i]        = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen raw", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tAlbedoTexture[i]           = pl__refr_create_texture(&tAlbedoTextureDesc, "albedo original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tNormalTexture[i]           = pl__refr_create_texture(&tNormalTextureDesc, "normal original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tAOMetalRoughnessTexture[i] = pl__refr_create_texture(&tEmmissiveTexDesc, "metalroughness original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tDepthTexture[i]            = pl__refr_create_texture(&tDepthTextureDesc,      "offscreen depth original", i, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);
        ptView->atUVMaskTexture0[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 0", i, PL_TEXTURE_USAGE_STORAGE);
        ptView->atUVMaskTexture1[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 1", i, PL_TEXTURE_USAGE_STORAGE);

        // buffers
        ptView->atGlobalBuffers[i] = pl__refr_create_staging_buffer(&atGlobalBuffersDesc, "global", i);
        
        // lighting bind group
        const plBindGroupDesc tLightingBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tLightingBindGroupLayout,
            .pcDebugName = "lighting bind group"
        };
        ptView->tLightingBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);

        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = ptView->tAlbedoTexture[i],
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tNormalTexture[i],
                .uSlot    = 1,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tAOMetalRoughnessTexture[i],
                .uSlot    = 2,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tDepthTexture[i],
                .uSlot    = 3,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 4,
            .atTextureBindings = atBGTextureData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->tLightingBindGroup[i], &tBGData);
        pl_temp_allocator_reset(&gptData->tTempAllocator);

        // attachment sets
        atPickAttachmentSets[i].atViewAttachments[0] = ptView->tPickDepthTexture;
        atPickAttachmentSets[i].atViewAttachments[1] = ptView->tPickTexture;

        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tRawOutputTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tAOMetalRoughnessTexture[i];

        atUVAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atUVAttachmentSets[i].atViewAttachments[1] = ptView->atUVMaskTexture0[i];
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atPostProcessAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture[i];

        ptView->atLightShadowDataBuffer[i] = pl__refr_create_staging_buffer(&atLightShadowDataBufferDesc, "shadow", i);
        ptView->tShadowData.atCameraBuffers[i] = pl__refr_create_staging_buffer(&atCameraBuffersDesc, "shadow buffer", i);
        ptView->tShadowData.tDepthTexture[i] = pl__refr_create_texture(&tShadowDepthTextureDesc, "shadow map", i, PL_TEXTURE_USAGE_SAMPLED);


        for(uint32_t j = 0; j < 4; j++)
        {

            plTextureViewDesc tShadowDepthView = {
                .tFormat     = PL_FORMAT_D32_FLOAT,
                .uBaseMip    = 0,
                .uMips       = 1,
                .uBaseLayer  = j,
                .uLayerCount = 1,
                .tTexture    = ptView->tShadowData.tDepthTexture[i],
                .pcDebugName = "shadow view"
            };
            (ptView->tShadowData.atDepthTextureViews[j])[i] = gptGfx->create_texture_view(gptData->ptDevice, &tShadowDepthView);
            (atShadowAttachmentSets[j])[i].atViewAttachments[0] = (ptView->tShadowData.atDepthTextureViews[j])[i];
        }
    }

    const plRenderPassDesc tRenderPassDesc = {
        .tLayout = gptData->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
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
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
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
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
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

    const plRenderPassDesc tDepthRenderPassDesc = {
        .tLayout = gptData->tDepthRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_STORE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_SAMPLED,
                .tNextUsage      = PL_TEXTURE_USAGE_SAMPLED,
                .fClearZ         = 1.0f
        },
        .tDimensions = {.x = ptView->tShadowData.tResolution.x, .y = ptView->tShadowData.tResolution.y}
    };

    // create offscreen renderpass
    const plRenderPassDesc tUVRenderPass0Desc = {
        .tLayout = gptData->tUVRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_LOAD,
                .tStencilStoreOp = PL_STORE_OP_STORE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
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

    for(uint32_t i = 0; i < 4; i++)
    {
        ptView->tShadowData.atOpaqueRenderPasses[i] = gptGfx->create_render_pass(gptData->ptDevice, &tDepthRenderPassDesc, atShadowAttachmentSets[i]);
    }

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

    const plTextureDesc tPickDepthTextureDesc = {
        .tDimensions   = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
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
        .tFormat       = PL_FORMAT_R8G8B8A8_SRGB,
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

    const plBindGroupLayout tLightingBindGroupLayout = {
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
            {.uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
        }
    };

    // update offscreen render pass attachments
    plRenderPassAttachments atAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atUVAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPostProcessAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};
    plRenderPassAttachments atPickAttachmentSets[PL_MAX_FRAMES_IN_FLIGHT] = {0};

    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickTexture);
    gptGfx->queue_texture_for_deletion(ptDevice, ptView->tPickDepthTexture);
    ptView->tPickTexture = pl__refr_create_texture(&tPickTextureDesc, "pick", 0, PL_TEXTURE_USAGE_SAMPLED);
    ptView->tPickDepthTexture = pl__refr_create_texture(&tPickDepthTextureDesc, "pick depth", 0, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {

        // queue old resources for deletion
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture0[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->atUVMaskTexture1[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tFinalTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tRawOutputTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAlbedoTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tNormalTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tAOMetalRoughnessTexture[i]);
        gptGfx->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture[i]);
        gptGfx->queue_bind_group_for_deletion(ptDevice, ptView->tLightingBindGroup[i]);

        // textures
        ptView->tFinalTexture[i]            = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen final", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tFinalTextureHandle[i]      = gptDrawBackend->create_bind_group_for_texture(ptView->tFinalTexture[i]);
        ptView->tRawOutputTexture[i]        = pl__refr_create_texture(&tRawOutputTextureDesc,  "offscreen raw", i, PL_TEXTURE_USAGE_SAMPLED);
        ptView->tAlbedoTexture[i]           = pl__refr_create_texture(&tAlbedoTextureDesc, "albedo original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tNormalTexture[i]           = pl__refr_create_texture(&tNormalTextureDesc, "normal resize", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tAOMetalRoughnessTexture[i] = pl__refr_create_texture(&tEmmissiveTexDesc, "metalroughness original", i, PL_TEXTURE_USAGE_COLOR_ATTACHMENT);
        ptView->tDepthTexture[i]            = pl__refr_create_texture(&tDepthTextureDesc,      "offscreen depth original", i, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT);
        ptView->atUVMaskTexture0[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 0", i, PL_TEXTURE_USAGE_STORAGE);
        ptView->atUVMaskTexture1[i]         = pl__refr_create_texture(&tMaskTextureDesc, "uv mask texture 1", i, PL_TEXTURE_USAGE_STORAGE);


        // lighting bind group
        plTempAllocator tTempAllocator = {0};
        const plBindGroupDesc tLightingBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tLightingBindGroupLayout,
            .pcDebugName = "lighting bind group"
        };
        ptView->tLightingBindGroup[i] = gptGfx->create_bind_group(gptData->ptDevice, &tLightingBindGroupDesc);
        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = ptView->tAlbedoTexture[i],
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tNormalTexture[i],
                .uSlot    = 1,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tAOMetalRoughnessTexture[i],
                .uSlot    = 2,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            },
            {
                .tTexture = ptView->tDepthTexture[i],
                .uSlot    = 3,
                .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 4,
            .atTextureBindings = atBGTextureData
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptView->tLightingBindGroup[i], &tBGData);
        pl_temp_allocator_free(&tTempAllocator);

        // attachment sets
        atPickAttachmentSets[i].atViewAttachments[0] = ptView->tPickDepthTexture;
        atPickAttachmentSets[i].atViewAttachments[1] = ptView->tPickTexture;

        atAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atAttachmentSets[i].atViewAttachments[1] = ptView->tRawOutputTexture[i];
        atAttachmentSets[i].atViewAttachments[2] = ptView->tAlbedoTexture[i];
        atAttachmentSets[i].atViewAttachments[3] = ptView->tNormalTexture[i];
        atAttachmentSets[i].atViewAttachments[4] = ptView->tAOMetalRoughnessTexture[i];
        
        atPostProcessAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atPostProcessAttachmentSets[i].atViewAttachments[1] = ptView->tFinalTexture[i];

        atUVAttachmentSets[i].atViewAttachments[0] = ptView->tDepthTexture[i];
        atUVAttachmentSets[i].atViewAttachments[1] = ptView->atUVMaskTexture0[i];
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
    {
        plRefScene* ptScene = &gptData->sbtScenes[i];
        for(uint32_t j = 0; j < ptScene->uViewCount; j++)
        {
            plRefView* ptView = &ptScene->atViews[j];
            pl_sb_free(ptView->sbtVisibleOpaqueDrawables);
            pl_sb_free(ptView->sbtVisibleTransparentDrawables);
            pl_sb_free(ptView->sbtLightShadowData);
        }
        pl_sb_free(ptScene->sbtLightData);
        pl_sb_free(ptScene->sbtVertexPosBuffer);
        pl_sb_free(ptScene->sbtVertexDataBuffer);
        pl_sb_free(ptScene->sbuIndexBuffer);
        pl_sb_free(ptScene->sbtMaterialBuffer);
        pl_sb_free(ptScene->sbtDeferredDrawables);
        pl_sb_free(ptScene->sbtForwardDrawables);
        pl_sb_free(ptScene->sbtSkinData);
        pl_sb_free(ptScene->sbtSkinVertexDataBuffer);
        pl_sb_free(ptScene->sbtOutlineDrawables);
        pl_sb_free(ptScene->sbtOutlineDrawablesOldShaders);
        pl_hm_free(ptScene->ptDeferredHashmap);
        pl_hm_free(ptScene->ptForwardHashmap);
        pl_hm_free(ptScene->ptMaterialHashmap);
        pl_hm_free(ptScene->ptTextureIndexHashmap);
        pl_hm_free(ptScene->ptCubeTextureIndexHashmap);
        gptECS->cleanup_component_library(&ptScene->tComponentLibrary);
    }
    for(uint32_t i = 0; i < pl_sb_size(gptData->_sbtVariantHandles); i++)
    {
        plShader* ptShader = gptGfx->get_shader(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
        gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
    }
    pl_sb_free(gptData->_sbtVariantHandles);
    pl_hm_free(gptData->ptVariantHashmap);
    gptGfx->flush_device(gptData->ptDevice);

    gptGfx->destroy_texture(gptData->ptDevice, gptData->tMSAATexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->cleanup_semaphore(gptData->aptSemaphores[i]);

    gptGfx->cleanup_bind_group_pool(gptData->ptBindGroupPool);
    gptGpuAllocators->cleanup(gptData->ptDevice);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_bind_group_pool(gptData->aptTempGroupPools[i]);
        gptGfx->cleanup_command_pool(gptData->atCmdPools[i]);
    }
    gptGfx->cleanup_swapchain(gptData->ptSwap);
    gptGfx->cleanup_surface(gptData->ptSurface);
    gptGfx->cleanup_device(gptData->ptDevice);
    gptGfx->cleanup();

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

    // create skybox shader if we haven't
    if(gptData->tSkyboxShader.uIndex == 0)
    {
        // create skybox shader
        plShaderDesc tSkyboxShaderDesc = {
            .tPixelShader = gptShader->load_glsl("../shaders/skybox.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("../shaders/skybox.vert", "main", NULL, NULL),
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
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE,  .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atSamplerBindings = {
                        {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    },
                    .atTextureBindings = {
                        {.uSlot = 5, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1},
                        {.uSlot = 7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 1}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
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

    ptScene->iEnvironmentMips = (uint32_t)floorf(log2f((float)pl_maxi(iResolution, iResolution))) - 3; // guarantee final dispatch during filtering is 16 threads

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);

    pl_begin_cpu_sample(gptProfile, 0, "step 0");
    {
        int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
        const plComputeShaderDesc tSkyboxComputeShaderDesc = {
            .tShader = gptShader->load_glsl("../shaders/panorama_to_cubemap.comp", "main", NULL, NULL),
            .pTempConstantData = aiSkyboxSpecializationData,
            .atConstants = {
                { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
                { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
                { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT}
            },
            .atBindGroupLayouts = {
                {
                    .atBufferBindings = {
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
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
            atComputeBuffers[i + 1] = pl__refr_create_local_buffer(&tOutputBufferDesc, "panorama output", i, NULL);

        plBindGroupLayout tComputeBindGroupLayout = {
            .atBufferBindings = {
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
            },
        };
        const plBindGroupDesc tComputeBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tComputeBindGroupLayout,
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
            { .tHandle = atComputeBuffers[0], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_READ },
            { .tHandle = atComputeBuffers[1], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[2], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[3], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[4], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[5], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atComputeBuffers[6], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tPassResources = {
            .uBufferCount = 7,
            .atBuffers = atPassBuffers
        };

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tPanoramaShader, 0, 1, &tComputeBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptComputeEncoder, tPanoramaShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
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
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

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
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        
        for(uint32_t i = 0; i < 7; i++)
            gptGfx->destroy_buffer(ptDevice, atComputeBuffers[i]);

        plBindGroupLayout tSkyboxBindGroupLayout = {
            .atTextureBindings = { {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}}
        };
    
        const plBindGroupDesc tSkyboxBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tSkyboxBindGroupLayout,
            .pcDebugName = "skybox bind group"
        };
        ptScene->tSkyboxBindGroup = gptGfx->create_bind_group(ptDevice, &tSkyboxBindGroupDesc);
        const plBindGroupUpdateTextureData tTextureData1 = {.tTexture = ptScene->tSkyboxTexture, .uSlot = 0, .uIndex = 0, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED};
        const plBindGroupUpdateData tBGData1 = {
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData1
        };
        gptGfx->update_bind_group(ptDevice, ptScene->tSkyboxBindGroup, &tBGData1);

        const uint32_t uStartIndex     = pl_sb_size(ptScene->sbtVertexPosBuffer);
        const uint32_t uIndexStart     = pl_sb_size(ptScene->sbuIndexBuffer);
        const uint32_t uDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);

        const plDrawable tDrawable = {
            .uIndexCount   = 36,
            .uVertexCount  = 8,
            .uIndexOffset  = uIndexStart,
            .uVertexOffset = uStartIndex,
            .uDataOffset   = uDataStartIndex,
        };
        ptScene->tSkyboxDrawable = tDrawable;

        // indices
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
        
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);

        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
        
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
        
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
        
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
        pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);

        // vertices (position)
        const float fCubeSide = 1.0f;
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide, -fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide,  fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide,  fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide,  fCubeSide}));
        pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide,  fCubeSide})); 
    }
    pl_end_cpu_sample(gptProfile, 0);

    pl_begin_cpu_sample(gptProfile, 0, "step 1");

    plComputeShaderDesc tFilterComputeShaderDesc = {
        .tShader = gptShader->load_glsl("../shaders/filter_environment.comp", "main", NULL, NULL),
        .atConstants = {
            { .uID = 0, .uOffset = 0,  .tType = PL_DATA_TYPE_INT},
            { .uID = 1, .uOffset = 4,  .tType = PL_DATA_TYPE_FLOAT},
            { .uID = 2, .uOffset = 8,  .tType = PL_DATA_TYPE_INT},
            { .uID = 3, .uOffset = 12, .tType = PL_DATA_TYPE_INT},
            { .uID = 4, .uOffset = 16, .tType = PL_DATA_TYPE_FLOAT},
            { .uID = 5, .uOffset = 20, .tType = PL_DATA_TYPE_INT},
            { .uID = 6, .uOffset = 24, .tType = PL_DATA_TYPE_INT},
            { .uID = 7, .uOffset = 28, .tType = PL_DATA_TYPE_INT}
        },
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                },
                .atBufferBindings = {
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 7, .tStages = PL_STAGE_COMPUTE},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 8, .tStages = PL_STAGE_COMPUTE},
                },
                .atSamplerBindings = { {.uSlot = 0, .tStages = PL_STAGE_COMPUTE}}
            }
        }
    };

    typedef struct _FilterShaderSpecData{
        int resolution;
        float u_roughness;
        int u_sampleCount;
        int u_width;
        float u_lodBias;
        int u_distribution;
        int u_isGeneratingLUT;
        int currentMipLevel;
    } FilterShaderSpecData;

    FilterShaderSpecData tFilterData0 = {
        .resolution = iResolution,
        .u_roughness = 0.0f,
        .u_width = iResolution,
        .u_distribution = 0,
        .u_lodBias = 0,
        .u_sampleCount = iSamples,
        .u_isGeneratingLUT = 0,
        .currentMipLevel = 0,
    };

    FilterShaderSpecData tFilterDatas[16] = {0};

    tFilterDatas[0].resolution = iResolution;
    tFilterDatas[0].u_roughness = 0.0f;
    tFilterDatas[0].u_width = iResolution;
    tFilterDatas[0].u_distribution = 1;
    tFilterDatas[0].u_lodBias = 1;
    tFilterDatas[0].u_sampleCount = iSamples;
    tFilterDatas[0].u_isGeneratingLUT = 1;
    tFilterDatas[0].currentMipLevel = 0;
    for(int i = 0; i < ptScene->iEnvironmentMips; i++)
    {
        int currentWidth = iResolution >> i;
        tFilterDatas[i + 1].resolution = iResolution;
        tFilterDatas[i + 1].u_roughness = (float)i / (float)(ptScene->iEnvironmentMips - 1);
        tFilterDatas[i + 1].u_width = currentWidth;
        tFilterDatas[i + 1].u_distribution = 1;
        tFilterDatas[i + 1].u_lodBias = 1;
        tFilterDatas[i + 1].u_sampleCount = iSamples;
        tFilterDatas[i + 1].u_isGeneratingLUT = 0; 
        tFilterDatas[i + 1].currentMipLevel = i; 
        // tVariants[i+1].pTempConstantData = &tFilterDatas[i + 1];
    }

    tFilterComputeShaderDesc.pTempConstantData = &tFilterData0;
    plComputeShaderHandle tIrradianceShader = gptGfx->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);

    tFilterComputeShaderDesc.pTempConstantData = &tFilterDatas[0];
    plComputeShaderHandle tLUTShader = gptGfx->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);

    plComputeShaderHandle atSpecularComputeShaders[16] = {0};
    for(int i = 0; i < ptScene->iEnvironmentMips + 1; i++)
    {
        tFilterComputeShaderDesc.pTempConstantData = &tFilterDatas[i + 1];
        atSpecularComputeShaders[i] = gptGfx->create_compute_shader(ptDevice, &tFilterComputeShaderDesc);
    }
    pl_temp_allocator_reset(&gptData->tTempAllocator);
    pl_end_cpu_sample(gptProfile, 0);

    // create lut
    
    {

        pl_begin_cpu_sample(gptProfile, 0, "step 2");
        plBufferHandle atLutBuffers[7] = {0};
        const plBufferDesc tInputBufferDesc = {
            .tUsage    = PL_BUFFER_USAGE_STORAGE | PL_BUFFER_USAGE_STAGING,
            .szByteSize = uFaceSize,
            .pcDebugName = "lut output buffer"
        };
        atLutBuffers[6] = pl__refr_create_staging_buffer(&tInputBufferDesc, "lut output", 0);

        for(uint32_t i = 0; i < 6; i++)
            atLutBuffers[i] = pl__refr_create_local_buffer(&tInputBufferDesc, "lut output", i, NULL);

        const plBindGroupDesc tFilterBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tFilterComputeShaderDesc.atBindGroupLayouts[0],
            .pcDebugName = "lut bind group"
        };
        plBindGroupHandle tLutBindGroup = gptGfx->create_bind_group(ptDevice, &tFilterBindGroupDesc);
        const plBindGroupUpdateBufferData atBGBufferData[] = {
            { .uSlot = 2, .tBuffer = atLutBuffers[0], .szBufferRange = uFaceSize},
            { .uSlot = 3, .tBuffer = atLutBuffers[1], .szBufferRange = uFaceSize},
            { .uSlot = 4, .tBuffer = atLutBuffers[2], .szBufferRange = uFaceSize},
            { .uSlot = 5, .tBuffer = atLutBuffers[3], .szBufferRange = uFaceSize},
            { .uSlot = 6, .tBuffer = atLutBuffers[4], .szBufferRange = uFaceSize},
            { .uSlot = 7, .tBuffer = atLutBuffers[5], .szBufferRange = uFaceSize},
            { .uSlot = 8, .tBuffer = atLutBuffers[6], .szBufferRange = uFaceSize},
        };

        const plBindGroupUpdateSamplerData tSamplerData = {
            .tSampler = gptData->tDefaultSampler,
            .uSlot = 0
        };
        const plBindGroupUpdateTextureData tTextureData = {
            .tTexture = ptScene->tSkyboxTexture,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        };
        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBufferBindings = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(ptDevice, tLutBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLutBindGroup);

        const plDispatch tDispach = {
            .uGroupCountX     = (uint32_t)iResolution / 16,
            .uGroupCountY     = (uint32_t)iResolution / 16,
            .uGroupCountZ     = 3,
            .uThreadPerGroupX = 16,
            .uThreadPerGroupY = 16,
            .uThreadPerGroupZ = 3
        };
        pl_end_cpu_sample(gptProfile, 0);

        pl_begin_cpu_sample(gptProfile, 0, "step 3");
        plBuffer* ptLutBuffer = gptGfx->get_buffer(ptDevice, atLutBuffers[6]);
        memset(ptLutBuffer->tMemoryAllocation.pHostMapped, 0, uFaceSize);
        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);

        const plPassBufferResource atPassBuffers[] = {
            { .tHandle = atLutBuffers[0], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atLutBuffers[1], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atLutBuffers[2], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atLutBuffers[3], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atLutBuffers[4], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atLutBuffers[5], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            { .tHandle = atLutBuffers[6], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
        };

        const plPassResources tPassResources = {
            .uBufferCount = 7,
            .atBuffers = atPassBuffers
        };

        plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tLUTShader, 0, 1, &tLutBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptComputeEncoder, tLUTShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        gptGfx->queue_compute_shader_for_deletion(ptDevice, tLUTShader);

        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 1,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_2D,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tGGXLUTTexture = pl__refr_create_texture_with_data(&tTextureDesc, "lut texture", 0, ptLutBuffer->tMemoryAllocation.pHostMapped, uFaceSize);
        pl_end_cpu_sample(gptProfile, 0);

        pl_begin_cpu_sample(gptProfile, 0, "step 4");
        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
        gptGfx->bind_compute_bind_groups(ptComputeEncoder, tIrradianceShader, 0, 1, &tLutBindGroup, 0, NULL);
        gptGfx->bind_compute_shader(ptComputeEncoder, tIrradianceShader);
        gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
        gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
        gptGfx->end_compute_pass(ptComputeEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
        gptGfx->queue_compute_shader_for_deletion(ptDevice, tIrradianceShader);

        const plTextureDesc tSpecularTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = 1,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tLambertianEnvTexture = pl__refr_create_texture(&tSpecularTextureDesc, "specular texture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

        ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);
        plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

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
            gptGfx->copy_buffer_to_texture(ptBlitEncoder, atLutBuffers[i], ptScene->tLambertianEnvTexture, 1, &tBufferImageCopy);
        }
        gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptBlitEncoder);
        gptGfx->end_command_recording(ptCommandBuffer);
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);

        for(uint32_t i = 0; i < 7; i++)
            gptGfx->destroy_buffer(ptDevice, atLutBuffers[i]);
        pl_end_cpu_sample(gptProfile, 0);
    }
    

    pl_begin_cpu_sample(gptProfile, 0, "step 5");
    {
        const plTextureDesc tTextureDesc = {
            .tDimensions = {(float)iResolution, (float)iResolution, 1},
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers     = 6,
            .uMips       = ptScene->iEnvironmentMips,
            .tType       = PL_TEXTURE_TYPE_CUBE,
            .tUsage      = PL_TEXTURE_USAGE_SAMPLED
        };
        ptScene->tGGXEnvTexture = pl__refr_create_texture(&tTextureDesc, "tGGXEnvTexture", uSceneHandle, PL_TEXTURE_USAGE_SAMPLED);

        const plBindGroupUpdateSamplerData tSamplerData = {
            .tSampler = gptData->tDefaultSampler,
            .uSlot = 0
        };
        const plBindGroupUpdateTextureData tTextureData = {
            .tTexture = ptScene->tSkyboxTexture,
            .uSlot    = 1,
            .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
        };

        const size_t uMaxFaceSize = (size_t)iResolution * (size_t)iResolution * 4 * sizeof(float);

        static const char* apcBufferNames[] = {
            "output buffer 0",
            "output buffer 1",
            "output buffer 2",
            "output buffer 3",
            "output buffer 4",
            "output buffer 5",
            "output buffer 6"
        };

        plBufferHandle atInnerComputeBuffers[7] = {0};
        for(uint32_t j = 0; j < 7; j++)
        {
            const plBufferDesc tOutputBufferDesc = {
                .tUsage    = PL_BUFFER_USAGE_STORAGE,
                .szByteSize = uMaxFaceSize,
                .pcDebugName = apcBufferNames[j]
            };
            atInnerComputeBuffers[j] = pl__refr_create_local_buffer(&tOutputBufferDesc, "inner buffer", j, NULL);
        }

        const plBindGroupDesc tFilterComputeBindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tFilterComputeShaderDesc.atBindGroupLayouts[0],
            .pcDebugName = "lut bindgroup"
        };
        plBindGroupHandle tLutBindGroup = gptGfx->create_bind_group(ptDevice, &tFilterComputeBindGroupDesc);
        
        const plBindGroupUpdateBufferData atBGBufferData[] = {
            { .uSlot = 2, .tBuffer = atInnerComputeBuffers[0], .szBufferRange = uMaxFaceSize},
            { .uSlot = 3, .tBuffer = atInnerComputeBuffers[1], .szBufferRange = uMaxFaceSize},
            { .uSlot = 4, .tBuffer = atInnerComputeBuffers[2], .szBufferRange = uMaxFaceSize},
            { .uSlot = 5, .tBuffer = atInnerComputeBuffers[3], .szBufferRange = uMaxFaceSize},
            { .uSlot = 6, .tBuffer = atInnerComputeBuffers[4], .szBufferRange = uMaxFaceSize},
            { .uSlot = 7, .tBuffer = atInnerComputeBuffers[5], .szBufferRange = uMaxFaceSize},
            { .uSlot = 8, .tBuffer = atInnerComputeBuffers[6], .szBufferRange = uMaxFaceSize}
        };

        const plBindGroupUpdateData tBGData = {
            .uBufferCount = 7,
            .atBufferBindings = atBGBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = &tSamplerData,
            .uTextureCount = 1,
            .atTextureBindings = &tTextureData
        };
        gptGfx->update_bind_group(ptDevice, tLutBindGroup, &tBGData);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLutBindGroup);

        for (int i = ptScene->iEnvironmentMips - 1; i != -1; i--)
        {
            int currentWidth = iResolution >> i;

            // const size_t uCurrentFaceSize = (size_t)currentWidth * (size_t)currentWidth * 4 * sizeof(float);

            const plDispatch tDispach = {
                .uGroupCountX     = (uint32_t)currentWidth / 16,
                .uGroupCountY     = (uint32_t)currentWidth / 16,
                .uGroupCountZ     = 2,
                .uThreadPerGroupX = 16,
                .uThreadPerGroupY = 16,
                .uThreadPerGroupZ = 3
            };

            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCommandBuffer, NULL);

            const plPassBufferResource atInnerPassBuffers[] = {
                { .tHandle = atInnerComputeBuffers[0], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
                { .tHandle = atInnerComputeBuffers[1], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
                { .tHandle = atInnerComputeBuffers[2], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
                { .tHandle = atInnerComputeBuffers[3], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
                { .tHandle = atInnerComputeBuffers[4], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
                { .tHandle = atInnerComputeBuffers[5], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
                { .tHandle = atInnerComputeBuffers[6], .tStages = PL_STAGE_COMPUTE, .tUsage = PL_PASS_RESOURCE_USAGE_WRITE },
            };

            const plPassResources tInnerPassResources = {
                .uBufferCount = 7,
                .atBuffers = atInnerPassBuffers
            };

            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, &tInnerPassResources);
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);
            gptGfx->bind_compute_bind_groups(ptComputeEncoder, atSpecularComputeShaders[i], 0, 1, &tLutBindGroup, 0, NULL);
            gptGfx->bind_compute_shader(ptComputeEncoder, atSpecularComputeShaders[i]);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
            gptGfx->end_compute_pass(ptComputeEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
            gptGfx->wait_on_command_buffer(ptCommandBuffer);
            gptGfx->return_command_buffer(ptCommandBuffer);
            gptGfx->queue_compute_shader_for_deletion(ptDevice, atSpecularComputeShaders[i]);

            ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCommandBuffer, NULL);
            plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

            for(uint32_t j = 0; j < 6; j++)
            {
                const plBufferImageCopy tBufferImageCopy = {
                    .uImageWidth = currentWidth,
                    .uImageHeight = currentWidth,
                    .uImageDepth = 1,
                    .uLayerCount     = 1,
                    .szBufferOffset  = 0,
                    .uBaseArrayLayer = j,
                    .uMipLevel       = i
                };
                gptGfx->copy_buffer_to_texture(ptBlitEncoder, atInnerComputeBuffers[j], ptScene->tGGXEnvTexture, 1, &tBufferImageCopy);
            }
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
            gptGfx->end_blit_pass(ptBlitEncoder);
            gptGfx->end_command_recording(ptCommandBuffer);
            gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
            gptGfx->wait_on_command_buffer(ptCommandBuffer);
            gptGfx->return_command_buffer(ptCommandBuffer);

        }
        for(uint32_t j = 0; j < 7; j++)
            gptGfx->queue_buffer_for_deletion(ptDevice, atInnerComputeBuffers[j]);
    }

    ptScene->uGGXLUT = pl__get_bindless_texture_index(uSceneHandle, ptScene->tGGXLUTTexture);
    ptScene->uLambertianEnvSampler = pl__get_bindless_cube_texture_index(uSceneHandle, ptScene->tLambertianEnvTexture);
    ptScene->uGGXEnvSampler = pl__get_bindless_cube_texture_index(uSceneHandle, ptScene->tGGXEnvTexture);

    pl_end_cpu_sample(gptProfile, 0);
    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_select_entities(uint32_t uSceneHandle, uint32_t uCount, plEntity* atEntities)
{
    // for convience
    plRefScene* ptScene    = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice   = gptData->ptDevice;

    if(uCount == 0)
        gptData->tPickedEntity = (plEntity){.ulData = UINT64_MAX};

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(ptScene->tGGXEnvTexture.uIndex != 0)
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

        // choose shader variant
        int aiConstantData0[5] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iSceneWideRenderingFlags
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

        size_t szSpecializationSize = 0;
        for(uint32_t j = 0; j < ptOutlineShader->tDesc._uConstantCount; j++)
        {
            const plSpecializationConstant* ptConstant = &ptOutlineShader->tDesc.atConstants[j];
            szSpecializationSize += pl__get_data_type_size2(ptConstant->tType);
        }

        // const uint64_t ulVariantHash = pl_hm_hash(tOutlineVariant.pTempConstantData, szSpecializationSize, tOutlineVariant.tGraphicsState.ulValue);
        // pl_hm_remove(&gptData->ptVariantHashmap, ulVariantHash);

        if(pl_hm_has_key(ptScene->ptDeferredHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptDeferredHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtDeferredDrawables[ulIndex];
            ptDrawable->tShader = ptScene->sbtOutlineDrawablesOldShaders[i];
        }
        else if(pl_hm_has_key(ptScene->ptForwardHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptForwardHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtForwardDrawables[ulIndex];
            ptDrawable->tShader = ptScene->sbtOutlineDrawablesOldShaders[i];
        }

        // gptGfx->queue_shader_for_deletion(ptDevice, ptScene->sbtOutlineDrawables[i].tShader);
    }
    pl_sb_reset(ptScene->sbtOutlineDrawables)
    pl_sb_reset(ptScene->sbtOutlineDrawablesOldShaders)



    for(uint32_t i = 0; i < uCount; i++)
    {
        plEntity tEntity = atEntities[i];

        gptData->tPickedEntity = tEntity;

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

        // choose shader variant
        const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
        int aiConstantData0[6] = {
            (int)ptMesh->ulVertexStreamMask,
            iDataStride,
            iTextureMappingFlags,
            PL_INFO_MATERIAL_METALLICROUGHNESS,
            iSceneWideRenderingFlags,
            pl_sb_size(sbtLights)
        };

        if(pl_hm_has_key(ptScene->ptDeferredHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptDeferredHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtDeferredDrawables[ulIndex];
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

            // plShaderHandle tOutlineShader = pl__get_shader_variant(uSceneHandle, gptData->tOutlineShader, &tOutlineVariant);
            pl_sb_push(ptScene->sbtOutlineDrawables, *ptDrawable);
            // ptScene->sbtOutlineDrawables[pl_sb_size(ptScene->sbtOutlineDrawables) - 1].tShader = tOutlineShader;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            pl_sb_push(ptScene->sbtOutlineDrawablesOldShaders, ptDrawable->tShader);
            ptDrawable->tShader = pl__get_shader_variant(uSceneHandle, gptData->tDeferredShader, &tVariant);
        }
        else if(pl_hm_has_key(ptScene->ptForwardHashmap, tEntity.ulData))
        {
            uint64_t ulIndex = pl_hm_lookup(ptScene->ptForwardHashmap, tEntity.ulData);
            plDrawable* ptDrawable = &ptScene->sbtForwardDrawables[ulIndex];
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

            // plShaderHandle tOutlineShader = pl__get_shader_variant(uSceneHandle, gptData->tOutlineShader, &tOutlineVariant);
            pl_sb_push(ptScene->sbtOutlineDrawables, *ptDrawable);
            // ptScene->sbtOutlineDrawables[pl_sb_size(ptScene->sbtOutlineDrawables) - 1].tShader = tOutlineShader;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            pl_sb_push(ptScene->sbtOutlineDrawablesOldShaders, ptDrawable->tShader);
            ptDrawable->tShader = pl__get_shader_variant(uSceneHandle, gptData->tForwardShader, &tVariant);
        }
    }
}

static void
pl_refr_reload_scene_shaders(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice = gptData->ptDevice;

    // fill CPU buffers & drawable list
    pl_begin_cpu_sample(gptProfile, 0, "recreate shaders");

    // old cleanup
    for(uint32_t i = 0; i < pl_sb_size(gptData->_sbtVariantHandles); i++)
    {
        plShader* ptShader = gptGfx->get_shader(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
        gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->_sbtVariantHandles[i]);
    }
    pl_sb_free(gptData->_sbtVariantHandles);
    pl_hm_free(gptData->ptVariantHashmap);

    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tDeferredShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tForwardShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tAlphaShadowShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tShadowShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tPickShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tUVShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, gptData->tSkyboxShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tLightingShader);
    gptGfx->queue_shader_for_deletion(gptData->ptDevice, ptScene->tTonemapShader);

    pl_refr_create_global_shaders();

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(ptScene->tGGXEnvTexture.uIndex != 0)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // create lighting shader
    {
        const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
        int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_size(sbtLights)};
        plShaderDesc tLightingShaderDesc = {
            .tPixelShader = gptShader->load_glsl("../shaders/lighting.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("../shaders/lighting.vert", "main", NULL, NULL),
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
                            .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                        },
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 1,
                            .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                        },
                    },
                    .atSamplerBindings = {
                        {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atTextureBindings = {
                        {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                        {.uSlot = 4100, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
                    },
                },
                {
                    .atBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atTextureBindings = {
                        {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 4},
                    },
                    .atSamplerBindings = {
                        {.uSlot = 6, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                }
            }
        };
        for(uint32_t i = 0; i < 2; i++)
        {
            tLightingShaderDesc.atConstants[i].uID = i;
            tLightingShaderDesc.atConstants[i].uOffset = i * sizeof(int);
            tLightingShaderDesc.atConstants[i].tType = PL_DATA_TYPE_INT;
        }
        ptScene->tLightingShader = gptGfx->create_shader(gptData->ptDevice, &tLightingShaderDesc);
    }

    const plShaderDesc tTonemapShaderDesc = {
        .tPixelShader = gptShader->load_glsl("../shaders/tonemap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/full_quad.vert", "main", NULL, NULL),
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
                    { .uSlot = 0, .tStages = PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    ptScene->tTonemapShader = gptGfx->create_shader(gptData->ptDevice, &tTonemapShaderDesc);

    plHashMap* ptMaterialBindGroupDict = {0};
    plBindGroupHandle* sbtMaterialBindGroups = NULL;
    plMaterialComponent* sbtMaterials = ptScene->tComponentLibrary.tMaterialComponentManager.pComponents;
    const uint32_t uMaterialCount = pl_sb_size(sbtMaterials);
    pl_sb_resize(sbtMaterialBindGroups, uMaterialCount);

    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        plMaterialComponent* ptMaterial = &sbtMaterials[i];
        pl_hm_insert(ptMaterialBindGroupDict, (uint64_t)ptMaterial, (uint64_t)i);
    }

    plDrawable* sbtDrawables[] = {
        ptScene->sbtDeferredDrawables,
        ptScene->sbtForwardDrawables,
    };

    plShaderHandle atTemplateShaders[] = {
        gptData->tDeferredShader,
        gptData->tForwardShader
    };

    plShaderHandle atTemplateShadowShaders[] = {
        gptData->tShadowShader,
        gptData->tAlphaShadowShader
    };

    plGraphicsState atTemplateVariants[] = {
        {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        }
    };
    
    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    for(uint32_t uDrawableBatchIndex = 0; uDrawableBatchIndex < 2; uDrawableBatchIndex++)
    {
        const uint32_t uDrawableCount = pl_sb_size(sbtDrawables[uDrawableBatchIndex]);
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {

            plEntity tEntity = (sbtDrawables[uDrawableBatchIndex])[i].tEntity;

            // get actual components
            plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
            plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
            plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);


            const uint64_t ulMaterialIndex = pl_hm_lookup(ptMaterialBindGroupDict, (uint64_t)ptMaterial);

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

            // choose shader variant
            int aiConstantData0[] = {
                (int)ptMesh->ulVertexStreamMask,
                iDataStride,
                iTextureMappingFlags,
                PL_INFO_MATERIAL_METALLICROUGHNESS,
                iSceneWideRenderingFlags,
                pl_sb_size(sbtLights)
            };

            plGraphicsState tVariantTemp = atTemplateVariants[uDrawableBatchIndex];

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            (sbtDrawables[uDrawableBatchIndex])[i].tShader = pl__get_shader_variant(uSceneHandle, atTemplateShaders[uDrawableBatchIndex], &tVariant);

            if(uDrawableBatchIndex > 0)
            {
                const plShaderVariant tShadowVariant = {
                    .pTempConstantData = aiConstantData0,
                    .tGraphicsState    = {
                        .ulDepthWriteEnabled  = 1,
                        .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
                        .ulCullMode           = PL_CULL_MODE_NONE,
                        .ulWireframe          = 0,
                        .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                        .ulStencilRef         = 0xff,
                        .ulStencilMask        = 0xff,
                        .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                        .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                        .ulStencilOpPass      = PL_STENCIL_OP_KEEP
                    }
                };
                (sbtDrawables[uDrawableBatchIndex])[i].tShadowShader = pl__get_shader_variant(uSceneHandle, atTemplateShadowShaders[uDrawableBatchIndex], &tShadowVariant);
            }
        }
    }

    pl_sb_free(sbtMaterialBindGroups);

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_finalize_scene(uint32_t uSceneHandle)
{
    // for convience
    plRefScene* ptScene    = &gptData->sbtScenes[uSceneHandle];
    plDevice*   ptDevice   = gptData->ptDevice;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_cpu_sample(gptProfile, 0, "load textures");
    plMaterialComponent* sbtMaterials = ptScene->tComponentLibrary.tMaterialComponentManager.pComponents;
    const uint32_t uMaterialCount = pl_sb_size(sbtMaterials);

    plAtomicCounter* ptCounter = NULL;
    plJobDesc tJobDesc = {
        .task  = pl__refr_job,
        .pData = sbtMaterials
    };
    gptJob->dispatch_batch(uMaterialCount, 0, tJobDesc, &ptCounter);
    gptJob->wait_for_counter(ptCounter);
    pl_end_cpu_sample(gptProfile, 0);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    int iSceneWideRenderingFlags = PL_RENDERING_FLAG_USE_PUNCTUAL;
    if(ptScene->tGGXEnvTexture.uIndex != 0)
        iSceneWideRenderingFlags |= PL_RENDERING_FLAG_USE_IBL;

    // fill CPU buffers & drawable list
    pl_begin_cpu_sample(gptProfile, 0, "create shaders");

    plDrawable* sbtDrawables[] = {
        ptScene->sbtDeferredDrawables,
        ptScene->sbtForwardDrawables,
    };

    plShaderHandle atTemplateShaders[] = {
        gptData->tDeferredShader,
        gptData->tForwardShader
    };

    plShaderHandle atTemplateShadowShaders[] = {
        gptData->tShadowShader,
        gptData->tAlphaShadowShader
    };

    plGraphicsState atTemplateVariants[] = {
        {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        }
    };

    plHashMap* atHashmaps[] = {
        ptScene->ptDeferredHashmap,
        ptScene->ptForwardHashmap
    };
    
    const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
    pl_sb_reserve(ptScene->sbtVertexDataBuffer, 40000000);
    pl_sb_reserve(ptScene->sbtVertexPosBuffer, 15000000);
    for(uint32_t uDrawableBatchIndex = 0; uDrawableBatchIndex < 2; uDrawableBatchIndex++)
    {
        plHashMap* ptHashmap = atHashmaps[uDrawableBatchIndex];
        const uint32_t uDrawableCount = pl_sb_size(sbtDrawables[uDrawableBatchIndex]);
        pl_hm_resize(ptHashmap, uDrawableCount);
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {

            (sbtDrawables[uDrawableBatchIndex])[i].uSkinIndex = UINT32_MAX;
            plEntity tEntity = (sbtDrawables[uDrawableBatchIndex])[i].tEntity;
            pl_hm_insert(ptHashmap, tEntity.ulData, i);

            // get actual components
            plObjectComponent*   ptObject   = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
            plMeshComponent*     ptMesh     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
            plMaterialComponent* ptMaterial = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

            uint32_t uMaterialIndex = UINT32_MAX;

            if(pl_hm_has_key(ptScene->ptMaterialHashmap, ptMesh->tMaterial.ulData))
            {
                uMaterialIndex = (uint32_t)pl_hm_lookup(ptScene->ptMaterialHashmap, ptMesh->tMaterial.ulData);
            }
            else
            {

                uint64_t ulValue = pl_hm_get_free_index(ptScene->ptMaterialHashmap);
                if(ulValue == UINT64_MAX)
                {
                    ulValue = pl_sb_size(ptScene->sbtMaterialBuffer);
                    pl_sb_add(ptScene->sbtMaterialBuffer);
                }

                uMaterialIndex = (uint32_t)ulValue;

                plTextureHandle tBaseColorTex = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_BASE_COLOR_MAP, true, 0);
                plTextureHandle tNormalTex = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_NORMAL_MAP, false, 0);
                plTextureHandle tEmissiveTex = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_EMISSIVE_MAP, true, 0);
                plTextureHandle tMetallicRoughnessTex = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP, false, 0);
                plTextureHandle tOcclusionTex = pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_OCCLUSION_MAP, false, 1);

                int iBaseColorTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tBaseColorTex);
                int iNormalTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tNormalTex);
                int iEmissiveTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tEmissiveTex);
                int iMetallicRoughnessTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tMetallicRoughnessTex);
                int iOcclusionTexIdx = (int)pl__get_bindless_texture_index(uSceneHandle, tOcclusionTex);

                plGPUMaterial tMaterial = {
                    .iMipCount = ptScene->iEnvironmentMips,
                    .fMetallicFactor = ptMaterial->fMetalness,
                    .fRoughnessFactor = ptMaterial->fRoughness,
                    .tBaseColorFactor = ptMaterial->tBaseColor,
                    .tEmissiveFactor = ptMaterial->tEmissiveColor.rgb,
                    .fAlphaCutoff = ptMaterial->fAlphaCutoff,
                    .fOcclusionStrength = 1.0f,
                    .fEmissiveStrength = 1.0f,
                    .iBaseColorUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].uUVSet,
                    .iNormalUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].uUVSet,
                    .iEmissiveUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_EMISSIVE_MAP].uUVSet,
                    .iOcclusionUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_OCCLUSION_MAP].uUVSet,
                    .iMetallicRoughnessUVSet = (int)ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP].uUVSet,
                    .iBaseColorTexIdx = iBaseColorTexIdx,
                    .iNormalTexIdx = iNormalTexIdx,
                    .iEmissiveTexIdx = iEmissiveTexIdx,
                    .iMetallicRoughnessTexIdx = iMetallicRoughnessTexIdx,
                    .iOcclusionTexIdx = iOcclusionTexIdx,
                };
                ptScene->sbtMaterialBuffer[uMaterialIndex] = tMaterial;
                pl_hm_insert(ptScene->ptMaterialHashmap, ptMesh->tMaterial.ulData, ulValue);
            }

            (sbtDrawables[uDrawableBatchIndex])[i].uMaterialIndex = uMaterialIndex;

            // add data to global buffers
            pl__add_drawable_data_to_global_buffer(ptScene, i, (sbtDrawables[uDrawableBatchIndex]));
            pl__add_drawable_skin_data_to_global_buffer(ptScene, i, (sbtDrawables[uDrawableBatchIndex]));

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

            // choose shader variant
            int aiConstantData0[] = {
                (int)ptMesh->ulVertexStreamMask,
                iDataStride,
                iTextureMappingFlags,
                PL_INFO_MATERIAL_METALLICROUGHNESS,
                iSceneWideRenderingFlags,
                pl_sb_size(sbtLights)
            };

            plGraphicsState tVariantTemp = atTemplateVariants[uDrawableBatchIndex];

            if(ptMaterial->tFlags & PL_MATERIAL_FLAG_DOUBLE_SIDED)
                tVariantTemp.ulCullMode = PL_CULL_MODE_NONE;

            const plShaderVariant tVariant = {
                .pTempConstantData = aiConstantData0,
                .tGraphicsState    = tVariantTemp
            };

            (sbtDrawables[uDrawableBatchIndex])[i].tShader = pl__get_shader_variant(uSceneHandle, atTemplateShaders[uDrawableBatchIndex], &tVariant);

            if(uDrawableBatchIndex > 0)
            {
                const plShaderVariant tShadowVariant = {
                    .pTempConstantData = aiConstantData0,
                    .tGraphicsState    = {
                        .ulDepthWriteEnabled  = 1,
                        .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
                        .ulCullMode           = PL_CULL_MODE_NONE,
                        .ulWireframe          = 0,
                        .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                        .ulStencilRef         = 0xff,
                        .ulStencilMask        = 0xff,
                        .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                        .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                        .ulStencilOpPass      = PL_STENCIL_OP_KEEP
                    }
                };
                (sbtDrawables[uDrawableBatchIndex])[i].tShadowShader = pl__get_shader_variant(uSceneHandle, atTemplateShadowShaders[uDrawableBatchIndex], &tShadowVariant);
            }
        }
        atHashmaps[uDrawableBatchIndex] = ptHashmap;
    }

    ptScene->ptDeferredHashmap = atHashmaps[0];
    ptScene->ptForwardHashmap = atHashmaps[1];

    pl_end_cpu_sample(gptProfile, 0);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~GPU Buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pl_begin_cpu_sample(gptProfile, 0, "fill GPU buffers");

    const plBufferDesc tShaderBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer),
        .pcDebugName = "shader buffer"
    };
    
    const plBufferDesc tIndexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_INDEX,
        .szByteSize = sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer),
        .pcDebugName = "index buffer"
    };
    
    const plBufferDesc tVertexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer),
        .pcDebugName = "vertex buffer"
    };
     
    const plBufferDesc tStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer),
        .pcDebugName = "storage buffer"
    };

    const plBufferDesc tSkinStorageBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STORAGE,
        .szByteSize = sizeof(plVec4) * pl_sb_size(ptScene->sbtSkinVertexDataBuffer),
        .pcDebugName = "skin buffer"
    };

    const plBufferDesc tLightBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_UNIFORM,
        .szByteSize = sizeof(plGPULight) * PL_MAX_LIGHTS,
        .pcDebugName = "light buffer"
    };

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptScene->atLightBuffer[i] = pl__refr_create_staging_buffer(&tLightBufferDesc, "light", i);

    ptScene->tMaterialDataBuffer = pl__refr_create_local_buffer(&tShaderBufferDesc,  "shader", uSceneHandle, ptScene->sbtMaterialBuffer);
    ptScene->tIndexBuffer        = pl__refr_create_local_buffer(&tIndexBufferDesc,   "index", uSceneHandle, ptScene->sbuIndexBuffer);
    ptScene->tVertexBuffer       = pl__refr_create_local_buffer(&tVertexBufferDesc,  "vertex", uSceneHandle, ptScene->sbtVertexPosBuffer);
    ptScene->tStorageBuffer      = pl__refr_create_local_buffer(&tStorageBufferDesc, "storage", uSceneHandle, ptScene->sbtVertexDataBuffer);

    if(tSkinStorageBufferDesc.szByteSize > 0)
    {
        ptScene->tSkinStorageBuffer  = pl__refr_create_local_buffer(&tSkinStorageBufferDesc, "skin storage", uSceneHandle, ptScene->sbtSkinVertexDataBuffer);

        const plBindGroupLayout tSkinBindGroupLayout0 = {
            .atSamplerBindings = {
                {.uSlot =  3, .tStages = PL_STAGE_COMPUTE}
            },
            .atBufferBindings = {
                { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
                { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
                { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_COMPUTE},
            }
        };
        const plBindGroupDesc tSkinBindGroupDesc = {
            .ptPool = gptData->ptBindGroupPool,
            .ptLayout = &tSkinBindGroupLayout0,
            .pcDebugName = "skin bind group"
        };
        ptScene->tSkinBindGroup0 = gptGfx->create_bind_group(ptDevice, &tSkinBindGroupDesc);

        const plBindGroupUpdateSamplerData atSamplerData[] = {
            { .uSlot = 3, .tSampler = gptData->tDefaultSampler}
        };
        const plBindGroupUpdateBufferData atBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptScene->tSkinStorageBuffer, .szBufferRange = tSkinStorageBufferDesc.szByteSize},
            { .uSlot = 1, .tBuffer = ptScene->tVertexBuffer,      .szBufferRange = tVertexBufferDesc.szByteSize},
            { .uSlot = 2, .tBuffer = ptScene->tStorageBuffer,     .szBufferRange = tStorageBufferDesc.szByteSize}

        };
        plBindGroupUpdateData tBGData0 = {
            .uBufferCount = 3,
            .atBufferBindings = atBufferData,
            .uSamplerCount = 1,
            .atSamplerBindings = atSamplerData,
        };
        gptGfx->update_bind_group(gptData->ptDevice, ptScene->tSkinBindGroup0, &tBGData0);
    }

    // create lighting shader
    {
        int aiLightingConstantData[] = {iSceneWideRenderingFlags, pl_sb_size(sbtLights)};
        plShaderDesc tLightingShaderDesc = {
            .tPixelShader = gptShader->load_glsl("../shaders/lighting.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("../shaders/lighting.vert", "main", NULL, NULL),
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
                            .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                        },
                        {
                            .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                            .uSlot = 1,
                            .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                        },
                    },
                    .atSamplerBindings = {
                        {.uSlot = 2, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atTextureBindings = {
                        {.uSlot = 4, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true},
                        {.uSlot = 4100, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = PL_MAX_BINDLESS_TEXTURES, .bNonUniformIndexing = true}
                    }
                },
                {
                    .atTextureBindings = {
                        { .uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT},
                        { .uSlot = 3, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT}
                    },
                },
                {
                    .atBufferBindings = {
                        { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                    .atTextureBindings = {
                        {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 4},
                    },
                    .atSamplerBindings = {
                        {.uSlot = 7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                    },
                }
            }
        };
        for(uint32_t i = 0; i < 2; i++)
        {
            tLightingShaderDesc.atConstants[i].uID = i;
            tLightingShaderDesc.atConstants[i].uOffset = i * sizeof(int);
            tLightingShaderDesc.atConstants[i].tType = PL_DATA_TYPE_INT;
        }
        ptScene->tLightingShader = gptGfx->create_shader(gptData->ptDevice, &tLightingShaderDesc);
    }

    const plShaderDesc tTonemapShaderDesc = {
        .tPixelShader = gptShader->load_glsl("../shaders/tonemap.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/full_quad.vert", "main", NULL, NULL),
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
                    { .uSlot = 0, .tStages = PL_STAGE_PIXEL}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED},
                    {.uSlot = 2, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    ptScene->tTonemapShader = gptGfx->create_shader(gptData->ptDevice, &tTonemapShaderDesc);

    pl_sb_free(ptScene->sbtVertexPosBuffer);
    pl_sb_free(ptScene->sbtVertexDataBuffer);
    pl_sb_free(ptScene->sbuIndexBuffer);

    plBuffer* ptStorageBuffer = gptGfx->get_buffer(ptDevice, ptScene->tStorageBuffer);


    const plBindGroupUpdateBufferData atGlobalBufferData[] = 
    {
        {
            .tBuffer       = ptScene->tStorageBuffer,
            .uSlot         = 0,
            .szBufferRange = ptStorageBuffer->tDesc.szByteSize
        },
        {
            .tBuffer       = ptScene->tMaterialDataBuffer,
            .uSlot         = 1,
            .szBufferRange = sizeof(plGPUMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)
        },
    };

    plBindGroupUpdateData tGlobalBindGroupData = {
        .uBufferCount = 2,
        .atBufferBindings = atGlobalBufferData
    };

    gptGfx->update_bind_group(gptData->ptDevice, ptScene->tGlobalBindGroup, &tGlobalBindGroupData);

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_run_ecs(uint32_t uSceneHandle)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    gptECS->run_script_update_system(&ptScene->tComponentLibrary);
    gptECS->run_animation_update_system(&ptScene->tComponentLibrary, gptIOI->get_io()->fDeltaTime);
    gptECS->run_transform_update_system(&ptScene->tComponentLibrary);
    gptECS->run_hierarchy_update_system(&ptScene->tComponentLibrary);
    gptECS->run_inverse_kinematics_update_system(&ptScene->tComponentLibrary);
    gptECS->run_skin_update_system(&ptScene->tComponentLibrary);
    gptECS->run_object_update_system(&ptScene->tComponentLibrary);
    pl_end_cpu_sample(gptProfile, 0);
}

static plEntity
pl_refr_get_picked_entity(void)
{
    return gptData->tPickedEntity;
}

static void
pl_refr_render_scene(uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    // for convience
    plCommandPool*     ptCmdPool = gptData->atCmdPools[gptGfx->get_current_frame_index()];
    plDevice*          ptDevice     = gptData->ptDevice;
    plDrawStream*      ptStream     = &gptData->tDrawStream;
    plRefScene*        ptScene      = &gptData->sbtScenes[uSceneHandle];
    plRefView*         ptView       = &ptScene->atViews[uViewHandle];
    plCameraComponent* ptCamera     = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *tOptions.ptViewCamera);
    plCameraComponent* ptCullCamera = tOptions.ptCullCamera ? gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_CAMERA, *tOptions.ptCullCamera) : ptCamera;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    if(!gptData->bFrustumCulling)
        ptCullCamera = NULL;

    pl_begin_cpu_sample(gptProfile, 0, "Scene Prep");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~culling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbtDeferredDrawables);
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbtForwardDrawables);

    plAtomicCounter* ptOpaqueCounter = NULL;
    plAtomicCounter* ptTransparentCounter = NULL;
    
    if(ptCullCamera)
    {
        // opaque objects
        plCullData tOpaqueCullData = {
            .ptScene      = ptScene,
            .ptCullCamera = ptCullCamera,
            .atDrawables  = ptScene->sbtDeferredDrawables
        };
        
        plJobDesc tOpaqueJobDesc = {
            .task  = pl__refr_cull_job,
            .pData = &tOpaqueCullData
        };

        gptJob->dispatch_batch(uOpaqueDrawableCount, 0, tOpaqueJobDesc, &ptOpaqueCounter);

        // transparent objects
        plCullData tTransparentCullData = {
            .ptScene      = ptScene,
            .ptCullCamera = ptCullCamera,
            .atDrawables  = ptScene->sbtForwardDrawables
        };
        
        plJobDesc tTransparentJobDesc = {
            .task = pl__refr_cull_job,
            .pData = &tTransparentCullData
        };
        gptJob->dispatch_batch(uTransparentDrawableCount, 0, tTransparentJobDesc, &ptTransparentCounter);
    }
    else 
    {
        if(pl_sb_size(ptView->sbtVisibleOpaqueDrawables) != uOpaqueDrawableCount)
        {
            pl_sb_resize(ptView->sbtVisibleOpaqueDrawables, uOpaqueDrawableCount);
            memcpy(ptView->sbtVisibleOpaqueDrawables, ptScene->sbtDeferredDrawables, sizeof(plDrawable) * uOpaqueDrawableCount);
        }
        if(pl_sb_size(ptView->sbtVisibleTransparentDrawables) != uTransparentDrawableCount)
        {
            pl_sb_resize(ptView->sbtVisibleTransparentDrawables, uTransparentDrawableCount);
            memcpy(ptView->sbtVisibleTransparentDrawables, ptScene->sbtForwardDrawables, sizeof(plDrawable) * uTransparentDrawableCount);
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const BindGroup_0 tBindGroupBuffer = {
        .tViewportSize         = {.xy = ptView->tTargetSize},
        .tCameraPos            = ptCamera->tPos,
        .tCameraProjection     = ptCamera->tProjMat,
        .tCameraView           = ptCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat),
        .uGGXEnvSampler        = ptScene->uGGXEnvSampler,
        .uGGXLUT               = ptScene->uGGXLUT,
        .uLambertianEnvSampler = ptScene->uLambertianEnvSampler
    };
    memcpy(gptGfx->get_buffer(ptDevice, ptView->atGlobalBuffers[uFrameIdx])->tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    const uint32_t uFrameIndex = gptGfx->get_current_frame_index();

    plBindGroupLayout tSkyboxViewBindGroupLayout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            }
        },
        .atSamplerBindings = {
            {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
        }
    };

    const plBindGroupDesc tSkyboxViewBindGroupDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIndex],
        .ptLayout    = &tSkyboxViewBindGroupLayout,
        .pcDebugName = "skybox view specific bindgroup"
    };
    plBindGroupHandle tSkyboxViewBindGroup = gptGfx->create_bind_group(ptDevice, &tSkyboxViewBindGroupDesc);

    plBindGroupLayout tViewBindGroupLayout = {
        .atBufferBindings = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            }
        }
    };

    const plBindGroupDesc tViewBindGroupDesc = {
        .ptPool      = gptData->aptTempGroupPools[uFrameIndex],
        .ptLayout    = &tViewBindGroupLayout,
        .pcDebugName = "view specific bindgroup"
    };
    plBindGroupHandle tViewBindGroup = gptGfx->create_bind_group(ptDevice, &tViewBindGroupDesc);

    plBindGroupUpdateSamplerData tSamplerData[] = {
        {
            .tSampler = gptData->tDefaultSampler,
            .uSlot    = 1
        }
    };

    const plBindGroupUpdateBufferData atNewBufferData[] = 
    {
        {
            .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0)
        }
    };

    const plBindGroupUpdateBufferData atBufferData[] = 
    {
        {
            .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
            .uSlot         = 0,
            .szBufferRange = sizeof(BindGroup_0)
        }
    };

    plBindGroupUpdateData tSkyboxViewBindGroupData = {
        .uBufferCount = 1,
        .atBufferBindings = atBufferData,
        .uSamplerCount = 1,
        .atSamplerBindings = tSamplerData,
    };
    plBindGroupUpdateData tViewBindGroupData = {
        .uBufferCount = 1,
        .atBufferBindings = atNewBufferData
    };

    gptGfx->update_bind_group(gptData->ptDevice, tSkyboxViewBindGroup, &tSkyboxViewBindGroupData);
    gptGfx->update_bind_group(gptData->ptDevice, tViewBindGroup, &tViewBindGroupData);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tSkyboxViewBindGroup);
    gptGfx->queue_bind_group_for_deletion(ptDevice, tViewBindGroup);
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update skin textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    uint64_t ulValue = gptData->aulNextTimelineValue[uFrameIdx];
    plTimelineSemaphore* tSemHandle = gptData->aptSemaphores[uFrameIdx];

    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_update_skin_textures(ptCommandBuffer, uSceneHandle);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~perform skinning~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_perform_skinning(ptCommandBuffer, uSceneHandle);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~generate shadow maps~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_generate_cascaded_shadow_map(ptCommandBuffer, uSceneHandle, uViewHandle, *tOptions.ptViewCamera, *tOptions.ptSunLight, gptData->fLambdaSplit);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

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
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~render scene~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 0 - g buffer fill~~~~~~~~~~~~~~~~~~~~~~~~~~

        plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tRenderPass, NULL);
        gptGfx->set_depth_bias(ptEncoder, 0.0f, 0.0f, 0.0f);

        gptJob->wait_for_counter(ptOpaqueCounter);
        pl_end_cpu_sample(gptProfile, 0); // prep scene
        if(ptCullCamera)
        {
            pl_sb_reset(ptView->sbtVisibleOpaqueDrawables);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uOpaqueDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtDeferredDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                    pl_sb_push(ptView->sbtVisibleOpaqueDrawables, tDrawable);
            }
        }

        const uint32_t uVisibleOpaqueDrawCount = pl_sb_size(ptView->sbtVisibleOpaqueDrawables);
        gptGfx->reset_draw_stream(ptStream, uVisibleOpaqueDrawCount);
        for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
        {
            const plDrawable tDrawable = ptView->sbtVisibleOpaqueDrawables[i];
            plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = tDrawable.tShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                .atBindGroups = {
                    ptScene->tGlobalBindGroup,
                    tViewBindGroup
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
        }

        gptGfx->draw_stream(ptEncoder, 1, &tArea);
        
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 1 - lighting~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        gptGfx->next_subpass(ptEncoder, NULL);

        plBuffer* ptShadowDataBuffer = gptGfx->get_buffer(ptDevice, ptView->atLightShadowDataBuffer[uFrameIdx]);
        memcpy(ptShadowDataBuffer->tMemoryAllocation.pHostMapped, ptView->sbtLightShadowData, sizeof(plGPULightShadowData) * pl_sb_size(ptView->sbtLightShadowData));
        
        const plLightComponent* sbtLights = ptScene->tComponentLibrary.tLightComponentManager.pComponents;
        pl_sb_reset(ptScene->sbtLightData);
        pl_sb_resize(ptScene->sbtLightData, pl_sb_size(sbtLights));
        int iShadowIndex = 0;
        for(uint32_t i = 0; i < pl_sb_size(sbtLights); i++)
        {
            const plLightComponent* ptLight = &sbtLights[i];

            const plGPULight tLight = {
                .fIntensity = ptLight->fIntensity,
                .fRange     = ptLight->fRange,
                .iType      = ptLight->tType,
                .tPosition  = ptLight->tPosition,
                .tDirection = ptLight->tDirection,
                .tColor     = ptLight->tColor,
                .iShadowIndex = ptLight->tFlags & PL_LIGHT_FLAG_CAST_SHADOW ? iShadowIndex++ : 0,
                .iCascadeCount = (int)ptLight->uCascadeCount
            };
            ptScene->sbtLightData[i] = tLight;
        }

        const plBindGroupLayout tLightBindGroupLayout2 = {
            .atBufferBindings = {
                { .uSlot = 0, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
                { .uSlot = 1, .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX},
                { .uSlot = 2, .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX}
            },
            .atTextureBindings = {
                {.uSlot = 3, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uDescriptorCount = 4},
            },
            .atSamplerBindings = {
                {.uSlot = 7, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
            },
        };
        const plBindGroupDesc tLightBGDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tLightBindGroupLayout2,
            .pcDebugName = "light bind group 2"
        };
        plBindGroupHandle tLightBindGroup2 = gptGfx->create_bind_group(ptDevice, &tLightBGDesc);

        const plBindGroupUpdateBufferData atLightBufferData[] = 
        {
            { .uSlot = 0, .tBuffer = ptView->atGlobalBuffers[uFrameIdx], .szBufferRange = sizeof(BindGroup_0) },
            { .uSlot = 1, .tBuffer = ptScene->atLightBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData)},
            { .uSlot = 2, .tBuffer = ptView->atLightShadowDataBuffer[uFrameIdx], .szBufferRange = sizeof(plGPULightShadowData) * pl_sb_size(ptView->sbtLightShadowData)}
        };
        plBindGroupUpdateTextureData atBGTextureData[4] = {0};
        for(uint32_t i = 0; i < 4; i++)
        {
            atBGTextureData[i].tTexture = (ptView->tShadowData.atDepthTextureViews[i])[uFrameIdx];
            atBGTextureData[i].uSlot = 3;
            atBGTextureData[i].uIndex = i;
            atBGTextureData[i].tType = PL_TEXTURE_BINDING_TYPE_SAMPLED;
        }

        plBindGroupUpdateSamplerData tShadowSamplerData[] = {
            {
                .tSampler = gptData->tShadowSampler,
                .uSlot    = 7
            }
        };

        plBindGroupUpdateData tBGData2 = {
            .uBufferCount = 3,
            .atBufferBindings = atLightBufferData,
            .uTextureCount = 4,
            .atTextureBindings = atBGTextureData,
            .uSamplerCount = 1,
            .atSamplerBindings = tShadowSamplerData
        };
        gptGfx->update_bind_group(gptData->ptDevice, tLightBindGroup2, &tBGData2);
        gptGfx->queue_bind_group_for_deletion(ptDevice, tLightBindGroup2);
        plBuffer* ptLightingBuffer = gptGfx->get_buffer(ptDevice, ptScene->atLightBuffer[uFrameIdx]);
        memcpy(ptLightingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtLightData, sizeof(plGPULight) * pl_sb_size(ptScene->sbtLightData));

        typedef struct _plLightingDynamicData{
            int iDataOffset;
            int iVertexOffset;
            int unused[2];
        } plLightingDynamicData;
        plDynamicBinding tLightingDynamicData = pl__allocate_dynamic_data(ptDevice);
        
        gptGfx->reset_draw_stream(ptStream, 1);
        pl_add_to_draw_stream(ptStream, (plDrawStreamData)
        {
            .tShader        = ptScene->tLightingShader,
            .auDynamicBuffers = {
                tLightingDynamicData.uBufferHandle
            },
            .atVertexBuffers = {
                gptData->tFullQuadVertexBuffer
            },
            .tIndexBuffer         = gptData->tFullQuadIndexBuffer,
            .uIndexOffset         = 0,
            .uTriangleCount       = 2,
            .atBindGroups = {
                ptScene->tGlobalBindGroup,
                ptView->tLightingBindGroup[uFrameIdx],
                tLightBindGroup2
            },
            .auDynamicBufferOffsets = {
                tLightingDynamicData.uByteOffset
            },
            .uInstanceOffset = 0,
            .uInstanceCount = 1
        });
        gptGfx->draw_stream(ptEncoder, 1, &tArea);
        
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~subpass 2 - forward~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        gptGfx->next_subpass(ptEncoder, NULL);

        if(ptScene->tSkyboxTexture.uIndex != 0)
        {
            
            plDynamicBinding tSkyboxDynamicData = pl__allocate_dynamic_data(ptDevice);
            plMat4* ptSkyboxDynamicData = (plMat4*)tSkyboxDynamicData.pcData;
            *ptSkyboxDynamicData = pl_mat4_translate_vec3(ptCamera->tPos);

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
                .tIndexBuffer         = ptScene->tIndexBuffer,
                .uIndexOffset         = ptScene->tSkyboxDrawable.uIndexOffset,
                .uTriangleCount       = ptScene->tSkyboxDrawable.uIndexCount / 3,
                .atBindGroups = {
                    tSkyboxViewBindGroup,
                    ptScene->tSkyboxBindGroup
                },
                .auDynamicBufferOffsets = {
                    tSkyboxDynamicData.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
            gptGfx->draw_stream(ptEncoder, 1, &tArea);
        }
        

        // transparent & complex material objects
        gptJob->wait_for_counter(ptTransparentCounter);
        if(ptCullCamera)
        {
            pl_sb_reset(ptView->sbtVisibleTransparentDrawables);
            for(uint32_t uDrawableIndex = 0; uDrawableIndex < uTransparentDrawableCount; uDrawableIndex++)
            {
                const plDrawable tDrawable = ptScene->sbtForwardDrawables[uDrawableIndex];
                if(!tDrawable.bCulled)
                    pl_sb_push(ptView->sbtVisibleTransparentDrawables, tDrawable);
            }
        }

        const uint32_t uVisibleTransparentDrawCount = pl_sb_size(ptView->sbtVisibleTransparentDrawables);
        gptGfx->reset_draw_stream(ptStream, uVisibleTransparentDrawCount);
        for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
        {
            const plDrawable tDrawable = ptView->sbtVisibleTransparentDrawables[i];
            plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
            plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
            
            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);

            DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
            ptDynamicData->iDataOffset = tDrawable.uDataOffset;
            ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
            ptDynamicData->tModel = ptTransform->tWorld;
            ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

            pl_add_to_draw_stream(ptStream, (plDrawStreamData)
            {
                .tShader        = tDrawable.tShader,
                .auDynamicBuffers = {
                    tDynamicBinding.uBufferHandle
                },
                .atVertexBuffers = {
                    ptScene->tVertexBuffer,
                },
                .tIndexBuffer         = tDrawable.uIndexCount == 0 ? (plBufferHandle){0} : ptScene->tIndexBuffer,
                .uIndexOffset         = tDrawable.uIndexOffset,
                .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
                .atBindGroups = {
                    ptScene->tGlobalBindGroup,
                    tLightBindGroup2
                },
                .auDynamicBufferOffsets = {
                    tDynamicBinding.uByteOffset
                },
                .uInstanceOffset = 0,
                .uInstanceCount = 1
            });
        }
        gptGfx->draw_stream(ptEncoder, 1, &tArea);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~debug drawing~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // bounding boxes
        const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlineDrawables);
        if(uOutlineDrawableCount > 0 && gptData->bShowSelectedBoundingBox)
        {
            const plVec4 tOutlineColor = (plVec4){0.0f, (float)sin(gptIOI->get_io()->dTime * 3.0) * 0.25f + 0.75f, 0.0f, 1.0f};
            for(uint32_t i = 0; i < uOutlineDrawableCount; i++)
            {
                const plDrawable tDrawable = ptScene->sbtOutlineDrawables[i];
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
                gptDraw->add_3d_aabb(ptView->pt3DSelectionDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tOutlineColor), .fThickness = 0.01f});
                
            }
        }

        // light drawing (temporary)
        for(uint32_t i = 0; i < pl_sb_size(ptScene->sbtLightData); i++)
        {
            if(ptScene->sbtLightData[i].iType == PL_LIGHT_TYPE_POINT)
            {
                const plVec4 tColor = {.rgb = ptScene->sbtLightData[i].tColor, .a = 1.0f};
                gptDraw->add_3d_cross(ptView->pt3DDrawList, ptScene->sbtLightData[i].tPosition, 0.02f, (plDrawLineOptions){.uColor = PL_COLOR_32_VEC4(tColor), .fThickness = 0.25f});
            }
        }

        // debug drawing
        if(gptData->bDrawAllBoundingBoxes)
        {
            for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtDeferredDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
            for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtForwardDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
        }
        else if(gptData->bDrawVisibleBoundingBoxes)
        {
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleOpaqueDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                plMeshComponent* ptMesh = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleTransparentDrawables[i].tEntity);

                gptDraw->add_3d_aabb(ptView->pt3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f), .fThickness = 0.02f});
            }
        }

        if(gptData->bShowOrigin)
        {
            const plMat4 tTransform = pl_identity_mat4();
            gptDraw->add_3d_transform(ptView->pt3DDrawList, &tTransform, 10.0f, (plDrawLineOptions){.fThickness = 0.02f});
        }

        if(ptCullCamera && ptCullCamera != ptCamera)
        {
            plDrawFrustumDesc tFrustumDesc = {
                .fAspectRatio = ptCullCamera->fAspectRatio,
                .fFarZ = ptCullCamera->fFarZ,
                .fNearZ = ptCullCamera->fNearZ,
                .fYFov = ptCullCamera->fFieldOfView
            };
            gptDraw->add_3d_frustum(ptView->pt3DSelectionDrawList, &ptCullCamera->tTransformMat, tFrustumDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.02f});
        }

        gptDrawBackend->submit_3d_drawlist(ptView->pt3DDrawList, ptEncoder, tDimensions.x, tDimensions.y, &tMVP, PL_DRAW_FLAG_REVERSE_Z_DEPTH | PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE, 1);
        gptDrawBackend->submit_3d_drawlist(ptView->pt3DSelectionDrawList, ptEncoder, tDimensions.x, tDimensions.y, &tMVP, 0, 1);
        gptGfx->end_render_pass(ptEncoder);

         //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity selection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        if(gptData->uClickedFrame == uFrameIdx && uViewHandle == 0)
        {
            gptData->uClickedFrame = UINT32_MAX;
            plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptView->tPickTexture);
            plBuffer* ptCachedStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tCachedStagingBuffer);
            const plVec2 tMousePos = gptIOI->get_mouse_pos();

            uint32_t uRowWidth = (uint32_t)ptTexture->tDesc.tDimensions.x * 4;
            uint32_t uPos = uRowWidth * (uint32_t)tMousePos.y + (uint32_t)tMousePos.x * 4;

            unsigned char* pucMapping2 = (unsigned char*)ptCachedStagingBuffer->tMemoryAllocation.pHostMapped;
            unsigned char* pucMapping = &pucMapping2[uPos];
            gptData->tPickedEntity.uIndex = pucMapping[0] + 256 * pucMapping[1] + 65536 * pucMapping[2];
            gptData->tPickedEntity.uGeneration = ptScene->tComponentLibrary.sbtEntityGenerations[gptData->tPickedEntity.uIndex];
        }

        bool bOwnMouse = gptUI->wants_mouse_capture();
        if(!bOwnMouse && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false) && uViewHandle == 0)
        {
            gptData->uClickedFrame = uFrameIdx;

            plBindGroupLayout tPickBindGroupLayout0 = {
                .atBufferBindings = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                }

            };
            const plBindGroupDesc tPickBGDesc = {
                .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
                .ptLayout    = &tPickBindGroupLayout0,
                .pcDebugName = "temp pick bind group"
            };
            plBindGroupHandle tPickBindGroup = gptGfx->create_bind_group(ptDevice, &tPickBGDesc);

            const plBindGroupUpdateBufferData atPickBufferData[] = 
            {
                {
                    .tBuffer       = ptView->atGlobalBuffers[uFrameIdx],
                    .uSlot         = 0,
                    .szBufferRange = sizeof(BindGroup_0)
                }
            };
            plBindGroupUpdateData tPickBGData0 = {
                .uBufferCount = 1,
                .atBufferBindings = atPickBufferData,
            };
            gptGfx->update_bind_group(gptData->ptDevice, tPickBindGroup, &tPickBGData0);
            gptGfx->queue_bind_group_for_deletion(ptDevice, tPickBindGroup);

            typedef struct _plPickDynamicData
            {
                plVec4 tColor;
                plMat4 tModel;
            } plPickDynamicData;
            ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tPickRenderPass, NULL);

            gptGfx->bind_shader(ptEncoder, gptData->tPickShader);
            gptGfx->bind_vertex_buffer(ptEncoder, ptScene->tVertexBuffer);
            for(uint32_t i = 0; i < uVisibleOpaqueDrawCount; i++)
            {
                const plDrawable tDrawable = ptView->sbtVisibleOpaqueDrawables[i];

                uint32_t uId = tDrawable.tEntity.uIndex;
                
                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
                plPickDynamicData* ptDynamicData = (plPickDynamicData*)tDynamicBinding.pcData;
                
                ptDynamicData->tColor = (plVec4){
                    ((float)(uId & 0x000000ff) / 255.0f),
                    ((float)((uId & 0x0000ff00) >>  8) / 255.0f),
                    ((float)((uId & 0x00ff0000) >> 16) / 255.0f),
                    1.0f};
                ptDynamicData->tModel = ptTransform->tWorld;

                gptGfx->bind_graphics_bind_groups(ptEncoder, gptData->tPickShader, 0, 1, &tPickBindGroup, 1, &tDynamicBinding);

                plDrawIndex tDraw = {
                    .tIndexBuffer = ptScene->tIndexBuffer,
                    .uIndexCount = tDrawable.uIndexCount,
                    .uIndexStart = tDrawable.uIndexOffset,
                    .uInstanceCount = 1
                };
                gptGfx->draw_indexed(ptEncoder, 1, &tDraw);
            }

            for(uint32_t i = 0; i < uVisibleTransparentDrawCount; i++)
            {
                const plDrawable tDrawable = ptView->sbtVisibleTransparentDrawables[i];

                plObjectComponent* ptObject = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
                plTransformComponent* ptTransform = gptECS->get_component(&ptScene->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
                
                plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
                plPickDynamicData* ptDynamicData = (plPickDynamicData*)tDynamicBinding.pcData;
                const uint32_t uId = tDrawable.tEntity.uIndex;
                ptDynamicData->tColor = (plVec4){
                    ((float)(uId & 0x000000ff) / 255.0f),
                    ((float)((uId & 0x0000ff00) >>  8) / 255.0f),
                    ((float)((uId & 0x00ff0000) >> 16) / 255.0f),
                    1.0f};
                ptDynamicData->tModel = ptTransform->tWorld;

                gptGfx->bind_graphics_bind_groups(ptEncoder, gptData->tPickShader, 0, 1, &tPickBindGroup, 1, &tDynamicBinding);

                plDrawIndex tDraw = {
                    .tIndexBuffer = ptScene->tIndexBuffer,
                    .uIndexCount = tDrawable.uIndexCount,
                    .uIndexStart = tDrawable.uIndexOffset,
                    .uInstanceCount = 1
                };
                gptGfx->draw_indexed(ptEncoder, 1, &tDraw);
            }
            gptGfx->end_render_pass(ptEncoder);

            plBuffer* ptCachedStagingBuffer = gptGfx->get_buffer(ptDevice, gptData->tCachedStagingBuffer);
            memset(ptCachedStagingBuffer->tMemoryAllocation.pHostMapped, 0, ptCachedStagingBuffer->tDesc.szByteSize);

            plBlitEncoder* ptBlitEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

            plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptView->tPickTexture);
            const plBufferImageCopy tBufferImageCopy = {
                .uImageWidth = (uint32_t)ptTexture->tDesc.tDimensions.x,
                .uImageHeight = (uint32_t)ptTexture->tDesc.tDimensions.y,
                .uImageDepth = 1,
                .uLayerCount = 1
            };
            gptGfx->copy_texture_to_buffer(ptBlitEncoder, ptView->tPickTexture, gptData->tCachedStagingBuffer, 1, &tBufferImageCopy);
            gptGfx->pipeline_barrier_blit(ptBlitEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
            gptGfx->end_blit_pass(ptBlitEncoder);
        }

        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

     //~~~~~~~~~~~~~~~~~~~~~~~~~~~~uv map pass for JFA~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, ptView->tUVRenderPass, NULL);

        // submit nonindexed draw using basic API
        gptGfx->bind_shader(ptEncoder, gptData->tUVShader);
        gptGfx->bind_vertex_buffer(ptEncoder, gptData->tFullQuadVertexBuffer);

        plDrawIndex tDraw = {
            .tIndexBuffer   = gptData->tFullQuadIndexBuffer,
            .uIndexCount    = 6,
            .uInstanceCount = 1,
        };
        gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

        // end render pass
        gptGfx->end_render_pass(ptEncoder);

        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

     //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~jump flood~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const uint32_t uOutlineDrawableCount = pl_sb_size(ptScene->sbtOutlineDrawables);
        

        // find next power of 2
        uint32_t uJumpDistance = 1;
        uint32_t uHalfWidth = gptData->uOutlineWidth / 2;
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
            .uGroupCountX     = (uint32_t)tDimensions.x / 8,
            .uGroupCountY     = (uint32_t)tDimensions.y / 8,
            .uGroupCountZ     = 1,
            .uThreadPerGroupX = 8,
            .uThreadPerGroupY = 8,
            .uThreadPerGroupZ = 1
        };

        const plBindGroupLayout tJFABindGroupLayout = {
            .atTextureBindings = {
                {.uSlot = 0, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE},
                {.uSlot = 1, .tStages = PL_STAGE_COMPUTE, .tType = PL_TEXTURE_BINDING_TYPE_STORAGE}
            }
        };

        const plBindGroupDesc tJFABindGroupDesc = {
            .ptPool      = gptData->aptTempGroupPools[gptGfx->get_current_frame_index()],
            .ptLayout    = &tJFABindGroupLayout,
            .pcDebugName = "temp jfa bind group"
        };
        plBindGroupHandle atJFABindGroups[] = {
            gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc),
            gptGfx->create_bind_group(gptData->ptDevice, &tJFABindGroupDesc)
        };

        const plBindGroupUpdateTextureData atJFATextureData0[] = 
        {
            {
                .tTexture = ptView->atUVMaskTexture0[uFrameIdx],
                .uSlot    = 0,
                .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                    .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
            },
            {
                .tTexture = ptView->atUVMaskTexture1[uFrameIdx],
                .uSlot    = 1,
                .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                    .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
            }
        };

        const plBindGroupUpdateTextureData atJFATextureData1[] = 
        {
            {
                .tTexture = ptView->atUVMaskTexture1[uFrameIdx],
                .uSlot    = 0,
                .tType    = PL_TEXTURE_BINDING_TYPE_STORAGE,
                .tCurrentUsage = PL_TEXTURE_USAGE_STORAGE
            },
            {
                .tTexture = ptView->atUVMaskTexture0[uFrameIdx],
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
            const plBeginCommandInfo tBeginInfo = {
                .uWaitSemaphoreCount   = 1,
                .atWaitSempahores      = {tSemHandle},
                .auWaitSemaphoreValues = {ulValue},
            };

            plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
            gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

            // begin main renderpass (directly to swapchain)
            plComputeEncoder* ptComputeEncoder = gptGfx->begin_compute_pass(ptCommandBuffer, NULL);
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE);

            ptView->tLastUVMask = (i % 2 == 0) ? ptView->atUVMaskTexture1[uFrameIdx] : ptView->atUVMaskTexture0[uFrameIdx];

            // submit nonindexed draw using basic API
            gptGfx->bind_compute_shader(ptComputeEncoder, gptData->tJFAShader);

            plDynamicBinding tDynamicBinding = pl__allocate_dynamic_data(ptDevice);
            plVec4* ptJumpDistance = (plVec4*)tDynamicBinding.pcData;
            ptJumpDistance->x = fJumpDistance;

            gptGfx->bind_compute_bind_groups(ptComputeEncoder, gptData->tJFAShader, 0, 1, &atJFABindGroups[i % 2], 1, &tDynamicBinding);
            gptGfx->dispatch(ptComputeEncoder, 1, &tDispach);

            // end render pass
            gptGfx->pipeline_barrier_compute(ptComputeEncoder, PL_STAGE_COMPUTE, PL_ACCESS_SHADER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE, PL_ACCESS_SHADER_READ);
            gptGfx->end_compute_pass(ptComputeEncoder);

            // end recording
            gptGfx->end_command_recording(ptCommandBuffer);

            const plSubmitInfo tSubmitInfo = {
                .uSignalSemaphoreCount   = 1,
                .atSignalSempahores      = {tSemHandle},
                .auSignalSemaphoreValues = {++ulValue},
            };
            gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
            gptGfx->return_command_buffer(ptCommandBuffer);

            fJumpDistance = fJumpDistance / 2.0f;
            if(fJumpDistance < 1.0f)
                fJumpDistance = 1.0f;
        }
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~post process~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    {
        const plBeginCommandInfo tBeginInfo = {
            .uWaitSemaphoreCount   = 1,
            .atWaitSempahores      = {tSemHandle},
            .auWaitSemaphoreValues = {ulValue},
        };

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);
        gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

        pl_refr_post_process_scene(ptCommandBuffer, uSceneHandle, uViewHandle, &tMVP);
        gptGfx->end_command_recording(ptCommandBuffer);

        const plSubmitInfo tSubmitInfo = {
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {tSemHandle},
            .auSignalSemaphoreValues = {++ulValue}
        };
        gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }
    gptData->aulNextTimelineValue[uFrameIdx] = ulValue;

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

    pl_end_cpu_sample(gptProfile, 0);
}

static void
pl_refr_resize(void)
{
    gptData->bReloadMSAA = true;
}

static bool
pl_refr_begin_frame(void)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

    gptGfx->begin_frame(gptData->ptDevice);

    if(gptData->bReloadSwapchain)
    {
        gptData->bReloadSwapchain = false;
        plSwapchainInit tDesc = {
            .bVSync  = gptData->bVSync,
            .uWidth  = (uint32_t)gptIO->tMainViewportSize.x,
            .uHeight = (uint32_t)gptIO->tMainViewportSize.y,
            .tSampleCount = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount
        };
        gptGfx->recreate_swapchain(gptData->ptSwap, &tDesc);
        
        pl_refr_resize();
        pl_end_cpu_sample(gptProfile, 0);
        return false;
    }

    if(gptData->bReloadMSAA)
    {
        gptData->bReloadMSAA = false;

        uint32_t uImageCount = 0;
        plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptData->ptSwap, &uImageCount);

        plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptData->atCmdPools[0]);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);

        // begin blit pass, copy buffer, end pass
        plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

        plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptData->ptSwap);
        const plTextureDesc tColorTextureDesc = {
            .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
            .tFormat       = tInfo.tFormat,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .pcDebugName   = "offscreen color texture",
            .tSampleCount  = tInfo.tSampleCount
        };

        gptGfx->queue_texture_for_deletion(gptData->ptDevice, gptData->tMSAATexture);

        // create textures
        gptData->tMSAATexture = gptGfx->create_texture(gptData->ptDevice, &tColorTextureDesc, NULL);

        // retrieve textures
        plTexture* ptColorTexture = gptGfx->get_texture(gptData->ptDevice, gptData->tMSAATexture);

        // allocate memory
        plDeviceMemoryAllocatorI* ptAllocator = gptData->ptLocalBuddyAllocator;
        if(ptColorTexture->tMemoryRequirements.ulSize > PL_DEVICE_BUDDY_BLOCK_SIZE)
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

        gptGfx->pipeline_barrier_blit(ptEncoder, PL_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_STAGE_VERTEX | PL_STAGE_COMPUTE | PL_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptEncoder);

        // finish recording
        gptGfx->end_command_recording(ptCommandBuffer);

        // submit command buffer
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);

        plRenderPassAttachments atMainAttachmentSets[16] = {0};
        for(uint32_t i = 0; i < uImageCount; i++)
        {
            atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
            atMainAttachmentSets[i].atViewAttachments[1] = gptData->tMSAATexture;
        }
        gptGfx->update_render_pass_attachments(gptData->ptDevice, gptData->tMainRenderPass, gptIO->tMainViewportSize, atMainAttachmentSets);

    }

    
    gptGfx->reset_command_pool(gptData->atCmdPools[gptGfx->get_current_frame_index()], 0);
    gptGfx->reset_bind_group_pool(gptData->aptTempGroupPools[gptGfx->get_current_frame_index()]);
    gptData->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptData->ptDevice);

    if(!gptGfx->acquire_swapchain_image(gptData->ptSwap))
    {
        plSwapchainInit tDesc = {
            .bVSync  = gptData->bVSync,
            .uWidth  = (uint32_t)gptIO->tMainViewportSize.x,
            .uHeight = (uint32_t)gptIO->tMainViewportSize.y,
            .tSampleCount = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount
        };
        gptGfx->recreate_swapchain(gptData->ptSwap, &tDesc);
        pl_refr_resize();
        pl_end_cpu_sample(gptProfile, 0);
        return false;
    }

    pl_end_cpu_sample(gptProfile, 0);
    return true;
}

static void
pl_refr_end_frame(void)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

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

    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, gptData->tMainRenderPass, NULL);

    // render ui
    pl_begin_cpu_sample(gptProfile, 0, "render ui");
    plIO* ptIO = gptIOI->get_io();
    gptUI->end_frame();
    gptDrawBackend->submit_2d_drawlist(gptUI->get_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount);
    gptDrawBackend->submit_2d_drawlist(gptUI->get_debug_draw_list(), ptEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount);
    pl_end_cpu_sample(gptProfile, 0);

    gptGfx->end_render_pass(ptEncoder);

    gptGfx->end_command_recording(ptCommandBuffer);

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {tSemHandle},
        .auSignalSemaphoreValues = {++ulValue},
    };
    gptData->aulNextTimelineValue[uFrameIdx] = ulValue;
    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, &gptData->ptSwap, 1))
    {
        plSwapchainInit tDesc = {
            .bVSync  = gptData->bVSync,
            .uWidth  = (uint32_t)gptIO->tMainViewportSize.x,
            .uHeight = (uint32_t)gptIO->tMainViewportSize.y,
            .tSampleCount = gptGfx->get_swapchain_info(gptData->ptSwap).tSampleCount
        };
        gptGfx->recreate_swapchain(gptData->ptSwap, &tDesc);
        pl_refr_resize();
    }

    gptGfx->return_command_buffer(ptCommandBuffer);
    pl_end_cpu_sample(gptProfile, 0);
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
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();
    return ptView->tFinalTextureHandle[uFrameIdx];
}

static void
pl_add_drawable_objects_to_scene(uint32_t uSceneHandle, uint32_t uDeferredCount, const plEntity* atDeferredObjects, uint32_t uForwardCount, const plEntity* atForwardObjects)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    #if 1
    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtForwardDrawables);
    pl_sb_add_n(ptScene->sbtForwardDrawables, uForwardCount);

    const uint32_t uOpaqueStart = pl_sb_size(ptScene->sbtDeferredDrawables);
    pl_sb_add_n(ptScene->sbtDeferredDrawables, uDeferredCount);

    for(uint32_t i = 0; i < uDeferredCount; i++)
        ptScene->sbtDeferredDrawables[uOpaqueStart + i].tEntity = atDeferredObjects[i];

    for(uint32_t i = 0; i < uForwardCount; i++)
        ptScene->sbtForwardDrawables[uTransparentStart + i].tEntity = atForwardObjects[i];
    #endif

    #if 0 // send through forward pass only
    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtForwardDrawables);
    pl_sb_add_n(ptScene->sbtForwardDrawables, uForwardCount + uDeferredCount);

    for(uint32_t i = 0; i < uDeferredCount; i++)
        ptScene->sbtForwardDrawables[uTransparentStart + i].tEntity = atDeferredObjects[i];

    for(uint32_t i = 0; i < uForwardCount; i++)
        ptScene->sbtForwardDrawables[uDeferredCount + uTransparentStart + i].tEntity = atForwardObjects[i];
    #endif

    #if 0 // send through deferred pass only
    const uint32_t uTransparentStart = pl_sb_size(ptScene->sbtDeferredDrawables);
    pl_sb_add_n(ptScene->sbtDeferredDrawables, uForwardCount + uDeferredCount);

    for(uint32_t i = 0; i < uDeferredCount; i++)
        ptScene->sbtDeferredDrawables[uTransparentStart + i].tEntity = atDeferredObjects[i];

    for(uint32_t i = 0; i < uForwardCount; i++)
        ptScene->sbtDeferredDrawables[uDeferredCount + uTransparentStart + i].tEntity = atForwardObjects[i];
    #endif
}

static void
pl_show_graphics_options(const char* pcTitle)
{
    if(gptUI->begin_collapsing_header(pcTitle, 0))
    {
        if(gptUI->checkbox("VSync", &gptData->bVSync))
            gptData->bReloadSwapchain = true;
        gptUI->checkbox("Show Origin", &gptData->bShowOrigin);
        gptUI->checkbox("Frustum Culling", &gptData->bFrustumCulling);
        gptUI->slider_float("Lambda Split", &gptData->fLambdaSplit, 0.0f, 1.0f, 0);
        gptUI->checkbox("Draw All Bounding Boxes", &gptData->bDrawAllBoundingBoxes);
        gptUI->checkbox("Draw Visible Bounding Boxes", &gptData->bDrawVisibleBoundingBoxes);
        gptUI->checkbox("Show Selected Bounding Box", &gptData->bShowSelectedBoundingBox);

        int iOutlineWidth  = (int)gptData->uOutlineWidth;
        if(gptUI->slider_int("Outline Width", &iOutlineWidth, 2, 50, 0))
        {
            gptData->uOutlineWidth = (uint32_t)iOutlineWidth;
        }
        gptUI->end_collapsing_header();
    }
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
        .initialize                    = pl_refr_initialize,
        .cleanup                       = pl_refr_cleanup,
        .create_scene                  = pl_refr_create_scene,
        .add_drawable_objects_to_scene = pl_add_drawable_objects_to_scene,
        .create_view                   = pl_refr_create_view,
        .run_ecs                       = pl_refr_run_ecs,
        .begin_frame                   = pl_refr_begin_frame,
        .end_frame                     = pl_refr_end_frame,
        .get_component_library         = pl_refr_get_component_library,
        .get_device                    = pl_refr_get_device,
        .get_swapchain                 = pl_refr_get_swapchain,
        .load_skybox_from_panorama     = pl_refr_load_skybox_from_panorama,
        .finalize_scene                = pl_refr_finalize_scene,
        .reload_scene_shaders          = pl_refr_reload_scene_shaders,
        .select_entities               = pl_refr_select_entities,
        .render_scene                  = pl_refr_render_scene,
        .get_view_color_texture        = pl_refr_get_view_color_texture,
        .resize_view                   = pl_refr_resize_view,
        .get_debug_drawlist            = pl_refr_get_debug_drawlist,
        .get_gizmo_drawlist            = pl_refr_get_gizmo_drawlist,
        .get_picked_entity             = pl_refr_get_picked_entity,
        .show_graphics_options         = pl_show_graphics_options,
        .get_command_pool              = pl__refr_get_command_pool,
        .resize                        = pl_refr_resize,
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

    gptECS         = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera      = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptUI          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptResource    = pl_get_api_latest(ptApiRegistry, plResourceI);
    #ifdef PL_CORE_EXTENSION_INCLUDE_SHADER
        gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);
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