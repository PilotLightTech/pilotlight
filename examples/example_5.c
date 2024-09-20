/*
   example_5.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates vertex buffers
     - demonstrates index buffers
     - demonstrates staging buffers
     - demonstrates graphics shaders
     - demonstrates indexed drawing
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
#include "pl_graphics_ext.h"
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

    // graphics & sync objects
    plDevice*         ptDevice;
    plSurface*        ptSurface;
    plSwapchain*      ptSwapchain;
    plSemaphoreHandle atSempahore[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t          aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;
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

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // set contexts again since we are now in a
        // differenct dll/so
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO      = ptApiRegistry->first(PL_API_IO);
        gptWindows = ptApiRegistry->first(PL_API_WINDOW);
        gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
        gptShader  = ptApiRegistry->first(PL_API_SHADER);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // create profiling & logging contexts (used by extension here)
    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();

    // add log channel (ignoring the return here)
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");
    
    // add these to data registry so they can be retrieved by extension
    // and subsequent app reloads
    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);

    // load extensions
    ptExtensionRegistry->load("pilot_light", NULL, NULL, true);
    
    // load required apis (NULL if not available)
    gptIO      = ptApiRegistry->first(PL_API_IO);
    gptWindows = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
    gptShader  = ptApiRegistry->first(PL_API_SHADER);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 5",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);

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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // vertex buffer data
    const float atVertexData[] = { // x, y, r, g, b, a
        -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f
    };

    // create vertex buffer
    const plBufferDescription tVertexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX,
        .uByteSize = sizeof(float) * 24
    };
    ptAppData->tVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, "vertex buffer");

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
        0, 1, 3,
        1, 2, 3
    };

    // create index buffer
    const plBufferDescription tIndexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_INDEX,
        .uByteSize = sizeof(uint32_t) * 6
    };
    ptAppData->tIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, "index buffer");

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
    const plBufferDescription tStagingBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STAGING,
        .uByteSize = 4096
    };
    ptAppData->tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, "staging buffer");

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

    // copy memory to mapped staging buffer
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, atVertexData, sizeof(float) * 24);
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[1024], atIndexData, sizeof(uint32_t) * 6);

    // begin recording
    plCommandBufferHandle tCommandBuffer = gptGfx->begin_command_recording(ptDevice, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoderHandle tEncoder = gptGfx->begin_blit_pass(tCommandBuffer);
    gptGfx->copy_buffer(tEncoder, ptAppData->tStagingBuffer, ptAppData->tVertexBuffer, 0, 0, sizeof(float) * 24);
    gptGfx->copy_buffer(tEncoder, ptAppData->tStagingBuffer, ptAppData->tIndexBuffer, 1024, 0, sizeof(uint32_t) * 6);
    gptGfx->end_blit_pass(tEncoder);

    // finish recording
    gptGfx->end_command_recording(tCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer_blocking(tCommandBuffer, NULL);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plShaderDescription tShaderDesc = {
        .tVertexShader = gptShader->load_glsl("../examples/shaders/example_4.vert", "main", NULL, NULL),
        .tPixelShader = gptShader->load_glsl("../examples/shaders/example_4.frag", "main", NULL, NULL),
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
        .tVertexBufferBinding = {
            .uByteStride = sizeof(float) * 6,
            .atAttributes = {
                {.uByteOffset = 0, .tFormat = PL_FORMAT_R32G32_FLOAT},
                {.uByteOffset = sizeof(float) * 2, .tFormat = PL_FORMAT_R32G32B32A32_FLOAT},
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled = false
            }
        },
        .uBlendStateCount = 1,
        .tRenderPassLayout = gptGfx->get_render_pass(ptDevice, gptGfx->get_main_render_pass(ptDevice))->tDesc.tLayout,
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
    gptGfx->destroy_shader(ptAppData->ptDevice, ptAppData->tShader);
    gptGfx->destroy_buffer(ptAppData->ptDevice, ptAppData->tVertexBuffer);
    gptGfx->destroy_buffer(ptAppData->ptDevice, ptAppData->tIndexBuffer);
    gptGfx->destroy_buffer(ptAppData->ptDevice, ptAppData->tStagingBuffer);
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
    plCommandBufferHandle tCommandBuffer = gptGfx->begin_command_recording(ptAppData->ptDevice, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoderHandle tEncoder = gptGfx->begin_render_pass(tCommandBuffer, gptGfx->get_main_render_pass(ptAppData->ptDevice));

    // submit nonindexed draw using basic API
    gptGfx->bind_shader(tEncoder, ptAppData->tShader);
    gptGfx->bind_vertex_buffer(tEncoder, ptAppData->tVertexBuffer);

    const plDrawIndex tDraw = {
        .uInstanceCount = 1,
        .uIndexCount    = 6,
        .tIndexBuffer   = ptAppData->tIndexBuffer
    };
    gptGfx->draw_indexed(tEncoder, 1, &tDraw);

    // end render pass
    gptGfx->end_render_pass(tEncoder);

    // end recording
    gptGfx->end_command_recording(tCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };

    if(!gptGfx->present(tCommandBuffer, &tSubmitInfo, ptAppData->ptSwapchain))
        gptGfx->resize(ptAppData->ptSwapchain);

    pl_end_profile_frame();
}