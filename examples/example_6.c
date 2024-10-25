/*
   example_6.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates vertex, index, staging buffers
     - demonstrates samplers, textures, bind groups
     - demonstrates graphics shaders
     - demonstrates indexed drawing
     - demonstrates image extension
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pl.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h" // not yet stable
#include "pl_image_ext.h"
#include "pl_shader_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // shaders
    plShaderHandle tShader;

    // buffers
    plBufferHandle tStagingBuffer;
    plBufferHandle tIndexBuffer;
    plBufferHandle tVertexBuffer;

    // textures
    plTextureHandle tTexture;

    // samplers
    plSamplerHandle tSampler;

    // bind groups
    plBindGroupHandle tBindGroup0;

    // graphics & sync objects
    plDevice*         ptDevice;
    plSurface*        ptSurface;
    plSwapchain*      ptSwapchain;
    plSemaphoreHandle atSempahore[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t          aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plCommandPool*    atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;
const plImageI*    gptImage   = NULL;
const plShaderI*   gptShader  = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);

    // set log & profile contexts
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO      = ptApiRegistry->first(PL_API_IO);
        gptWindows = ptApiRegistry->first(PL_API_WINDOW);
        gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
        gptImage   = ptApiRegistry->first(PL_API_IMAGE);
        gptShader  = ptApiRegistry->first(PL_API_SHADER);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // load extensions
    ptExtensionRegistry->load("pilot_light", NULL, NULL, true);
    
    // load required apis (NULL if not available)
    gptIO      = ptApiRegistry->first(PL_API_IO);
    gptWindows = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
    gptImage   = ptApiRegistry->first(PL_API_IMAGE);
    gptShader  = ptApiRegistry->first(PL_API_SHADER);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 6",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create_window(&tWindowDesc, &ptAppData->ptWindow);

    // initialize graphics system
    const plGraphicsInit tGraphicsInit = {
        .tFlags = PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED | PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED | PL_GRAPHICS_INIT_FLAGS_LOGGING_WARNING 
    };
    gptGfx->initialize(&tGraphicsInit);
    ptAppData->ptSurface = gptGfx->create_surface(ptAppData->ptWindow);

    // find suitable device
    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    gptGfx->enumerate_devices(atDeviceInfos, &uDeviceCount);

    // we will prefer discrete, then integrated
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
    atDeviceInfos[iBestDvcIdx].ptSurface = ptAppData->ptSurface;
    ptAppData->ptDevice = gptGfx->create_device(&atDeviceInfos[iBestDvcIdx]);

    // create swapchain
    const plSwapchainInit tSwapInit = {
        .ptSurface = ptAppData->ptSurface
    };
    ptAppData->ptSwapchain = gptGfx->create_swapchain(ptAppData->ptDevice, &tSwapInit);

    // initialize shader extension
    static const plShaderOptions tDefaultShaderOptions = {
        .uIncludeDirectoriesCount = 1,
        .apcIncludeDirectories = {
            "../examples/shaders/"
        }
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // for convience
    plDevice* ptDevice = ptAppData->ptDevice;

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atSempahore[i] = gptGfx->create_semaphore(ptDevice, false);

    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        ptAppData->atCmdPools[i] = gptGfx->create_command_pool(ptAppData->ptDevice);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // vertex buffer data
    const float atVertexData[] = { // x, y, u, v
        -0.5f, -0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f, 
         0.5f,  0.5f, 1.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 0.0f
    };

    // create vertex buffer
    const plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX,
        .szByteSize  = sizeof(float) * 16,
        .pcDebugName = "vertex buffer"
    };
    ptAppData->tVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tVertexBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tVertexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "vertex buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tVertexBuffer, &tVertexBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // index buffer data
    const uint32_t atIndexData[] = {
        0, 1, 2,
        0, 2, 3
    };

    // create index buffer
    const plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX,
        .szByteSize  = sizeof(uint32_t) * 6,
        .pcDebugName = "index buffer"
    };
    ptAppData->tIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tIndexBuffer);

    // allocate memory for the index buffer
    const plDeviceMemoryAllocation tIndexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "index buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tIndexBuffer, &tIndexBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~staging buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create vertex buffer
    const plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_STAGING,
        .szByteSize  = 640000,
        .pcDebugName = "staging buffer"
    };
    ptAppData->tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tStagingBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU_CPU,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tStagingBuffer, &tStagingBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~transfers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, atVertexData, sizeof(float) * 16);
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[1024], atIndexData, sizeof(uint32_t) * 6);

    // begin recording
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptAppData->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->copy_buffer(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tVertexBuffer, 0, 0, sizeof(float) * 16);
    gptGfx->copy_buffer(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tIndexBuffer, 1024, 0, sizeof(uint32_t) * 6);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // load image from disk
    int iImageWidth = 0;
    int iImageHeight = 0;
    int _unused;
    unsigned char* pucImageData = gptImage->load("../data/pilotlight-assets-master/textures/SpriteMapExample.png",
        &iImageWidth, &iImageHeight, &_unused, 4);

    // create texture
    const plTextureDesc tTextureDesc = {
        .tDimensions = { (float)iImageWidth, (float)iImageHeight, 1},
        .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "texture"
    };
    ptAppData->tTexture = gptGfx->create_texture(ptDevice, &tTextureDesc);

    // retrieve new texture
    plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptAppData->tTexture);

    // allocate memory
    const plDeviceMemoryAllocation tTextureAllocation = gptGfx->allocate_memory(ptDevice,
        ptTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        "texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tTexture, &tTextureAllocation);

    // copy memory to mapped staging buffer
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pucImageData, iImageWidth * iImageHeight * 4);

    // begin recording
    gptGfx->reset_command_buffer(ptCommandBuffer);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy data, end pass
    ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);

    const plBufferImageCopy tBufferImageCopy = {
        .tImageExtent = {(uint32_t)iImageWidth, (uint32_t)iImageHeight, 1},
        .uLayerCount = 1
    };

    gptGfx->copy_buffer_to_texture(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tTexture, 1, &tBufferImageCopy);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);

    // free image data
    gptImage->free(pucImageData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~samplers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plSamplerDesc tSamplerDesc = {
        .tMagFilter    = PL_FILTER_LINEAR,
        .tMinFilter    = PL_FILTER_LINEAR,
        .fMinMip       = 0.0f,
        .fMaxMip       = 1.0f,
        .tVAddressMode = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode = PL_ADDRESS_MODE_WRAP,
        .pcDebugName   = "sampler"
    };
    ptAppData->tSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create bind group
    const plBindGroupLayout tBindGroupLayout = {
        .uSamplerBindingCount = 1,
        .atSamplerBindings = {
            { .uSlot = 0, .tStages = PL_STAGE_PIXEL}
        },
        .uTextureBindingCount = 1,
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        }
    };
    ptAppData->tBindGroup0 = gptGfx->create_bind_group(ptDevice, &tBindGroupLayout, "bind group 0");

    // update bind group (actually point descriptors to GPU resources)
    const plBindGroupUpdateSamplerData tSamplerData = {
        .tSampler = ptAppData->tSampler,
        .uSlot = 0
    };

    const plBindGroupUpdateTextureData tTextureData = {
        .tTexture = ptAppData->tTexture,
        .uSlot    = 1,
        .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
    };

    const plBindGroupUpdateData tBGData = {
        .uSamplerCount = 1,
        .atSamplerBindings = &tSamplerData,
        .uTextureCount = 1,
        .atTextureBindings = &tTextureData
    };
    gptGfx->update_bind_group(ptDevice, ptAppData->tBindGroup0, &tBGData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plShaderDescription tShaderDesc = {
        .tVertexShader = gptShader->load_glsl("../examples/shaders/example_6.vert", "main", NULL, NULL),
        .tPixelShader = gptShader->load_glsl("../examples/shaders/example_6.frag", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tVertexBufferLayout = {
            .uByteStride = sizeof(float) * 4,
            .atAttributes = {
                {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32_FLOAT},
                {.uByteOffset = sizeof(float) * 2, .tFormat = PL_FORMAT_R32G32_FLOAT},
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled = false
            }
        },
        .uBlendStateCount = 1,
        .tRenderPassLayout = gptGfx->get_render_pass(ptDevice, gptGfx->get_main_render_pass(ptDevice))->tDesc.tLayout,
        .uBindGroupLayoutCount = 1,
        .atBindGroupLayouts = {
            {
                .uSamplerBindingCount = 1,
                .atSamplerBindings = {
                    { .uSlot = 0, .tStages = PL_STAGE_PIXEL}
                },
                .uTextureBindingCount = 1,
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        }
    };
    ptAppData->tShader = gptGfx->create_shader(ptDevice, &tShaderDesc);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptGfx->cleanup_command_pool(ptAppData->atCmdPools[i]);
    gptGfx->destroy_shader(ptAppData->ptDevice, ptAppData->tShader);
    gptGfx->destroy_buffer(ptAppData->ptDevice, ptAppData->tVertexBuffer);
    gptGfx->destroy_buffer(ptAppData->ptDevice, ptAppData->tIndexBuffer);
    gptGfx->destroy_buffer(ptAppData->ptDevice, ptAppData->tStagingBuffer);
    gptGfx->destroy_texture(ptAppData->ptDevice, ptAppData->tTexture);
    gptGfx->cleanup_swapchain(ptAppData->ptSwapchain);
    gptGfx->cleanup_surface(ptAppData->ptSurface);
    gptGfx->cleanup_device(ptAppData->ptDevice);
    gptGfx->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // perform any operations required during a window resize
    gptGfx->resize(ptAppData->ptSwapchain); // recreates swapchain
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    pl_begin_profile_frame();

    gptIO->new_frame();

    // begin new frame
    if(!gptGfx->begin_frame(ptAppData->ptSwapchain))
    {
        gptGfx->resize(ptAppData->ptSwapchain);
        pl_end_profile_frame();
        return;
    }

    plCommandPool* ptCmdPool = ptAppData->atCmdPools[gptGfx->get_current_frame_index()];
    gptGfx->reset_command_pool(ptCmdPool);
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);

    //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~

    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();

    // expected timeline semaphore values
    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    ptAppData->aulNextTimelineValue[uCurrentFrameIndex] = ulValue1;

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder* ptEncoder = gptGfx->begin_render_pass(ptCommandBuffer, gptGfx->get_main_render_pass(ptAppData->ptDevice));

    // submit nonindexed draw using basic API
    gptGfx->bind_shader(ptEncoder, ptAppData->tShader);
    gptGfx->bind_vertex_buffer(ptEncoder, ptAppData->tVertexBuffer);

    // retrieve dynamic binding data
    plDynamicBinding tDynamicBinding = gptGfx->allocate_dynamic_data(ptAppData->ptDevice, sizeof(plVec4));
    plVec4* tTintColor = (plVec4*)tDynamicBinding.pcData;
    tTintColor->r = 1.0f;
    tTintColor->g = 1.0f;
    tTintColor->b = 1.0f;
    tTintColor->a = 1.0f;

    // bind groups (up to 3 bindgroups + 1 dynamic binding are allowed)
    gptGfx->bind_graphics_bind_groups(ptEncoder, ptAppData->tShader, 0, 1, &ptAppData->tBindGroup0, 1, &tDynamicBinding);

    const plDrawIndex tDraw = {
        .uInstanceCount = 1,
        .uIndexCount    = 6,
        .tIndexBuffer   = ptAppData->tIndexBuffer
    };
    gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    // end render pass
    gptGfx->end_render_pass(ptEncoder);

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };

    if(!gptGfx->present(ptCommandBuffer, &tSubmitInfo, ptAppData->ptSwapchain))
        gptGfx->resize(ptAppData->ptSwapchain);

    gptGfx->return_command_buffer(ptCommandBuffer);
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_LOG_IMPLEMENTATION
#include "pl_log.h"
#undef PL_LOG_IMPLEMENTATION

#define PL_PROFILE_IMPLEMENTATION
#include "pl_profile.h"
#undef PL_PROFILE_IMPLEMENTATION