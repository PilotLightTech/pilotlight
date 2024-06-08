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
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
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
    plGraphics        tGraphics;
    plSemaphoreHandle atSempahore[PL_FRAMES_IN_FLIGHT];
    uint64_t          aulNextTimelineValue[PL_FRAMES_IN_FLIGHT];

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;
const plDeviceI*   gptDevice  = NULL;
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

    // retrieve the memory context (provided by the runtime) and
    // set it to allow for memory tracking when using PL_ALLOC/PL_FREE
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

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
        gptDevice  = ptApiRegistry->first(PL_API_DEVICE);
        gptImage   = ptApiRegistry->first(PL_API_IMAGE);
        gptShader  = ptApiRegistry->first(PL_API_SHADER);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here (using PL_ALLOC for memory tracking)
    ptAppData = PL_ALLOC(sizeof(plAppData));
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
    ptExtensionRegistry->load("pl_graphics_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_image_ext",    NULL, NULL, false);
    ptExtensionRegistry->load("pl_shader_ext",   NULL, NULL, false);
    
    // load required apis (NULL if not available)
    gptIO      = ptApiRegistry->first(PL_API_IO);
    gptWindows = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice  = ptApiRegistry->first(PL_API_DEVICE);
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
    ptAppData->ptWindow = gptWindows->create_window(&tWindowDesc);

    // initialize graphics system
    const plGraphicsDesc tGraphicsDesc = {
        .bEnableValidation = true
    };
    gptGfx->initialize(ptAppData->ptWindow, &tGraphicsDesc, &ptAppData->tGraphics);

    // initialize shader extension
    const plShaderExtInit tShaderInit = {
        .pcIncludeDirectory = "../examples/shaders/"
    };
    gptShader->initialize(&tShaderInit);

    // for convience
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
        ptAppData->atSempahore[i] = gptDevice->create_semaphore(ptDevice, false);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // vertex buffer data
    const float atVertexData[] = { // x, y, u, v
        -0.5f, -0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f, 
         0.5f,  0.5f, 1.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 0.0f
    };

    // create vertex buffer
    const plBufferDescription tVertexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX,
        .uByteSize = sizeof(float) * 16
    };
    ptAppData->tVertexBuffer = gptDevice->create_buffer(ptDevice, &tVertexBufferDesc, "vertex buffer");

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptVertexBuffer = gptDevice->get_buffer(ptDevice, ptAppData->tVertexBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tVertexBufferAllocation = gptDevice->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "vertex buffer memory");

    // bind the buffer to the new memory allocation
    gptDevice->bind_buffer_to_memory(ptDevice, ptAppData->tVertexBuffer, &tVertexBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // index buffer data
    const uint32_t atIndexData[] = {
        0, 1, 2,
        0, 2, 3
    };

    // create index buffer
    const plBufferDescription tIndexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_INDEX,
        .uByteSize = sizeof(uint32_t) * 6
    };
    ptAppData->tIndexBuffer = gptDevice->create_buffer(ptDevice, &tIndexBufferDesc, "index buffer");

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptIndexBuffer = gptDevice->get_buffer(ptDevice, ptAppData->tIndexBuffer);

    // allocate memory for the index buffer
    const plDeviceMemoryAllocation tIndexBufferAllocation = gptDevice->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "index buffer memory");

    // bind the buffer to the new memory allocation
    gptDevice->bind_buffer_to_memory(ptDevice, ptAppData->tIndexBuffer, &tIndexBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~staging buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create vertex buffer
    const plBufferDescription tStagingBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_STAGING,
        .uByteSize = 640000
    };
    ptAppData->tStagingBuffer = gptDevice->create_buffer(ptDevice, &tStagingBufferDesc, "staging buffer");

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, ptAppData->tStagingBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tStagingBufferAllocation = gptDevice->allocate_memory(ptDevice,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU_CPU,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptDevice->bind_buffer_to_memory(ptDevice, ptAppData->tStagingBuffer, &tStagingBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~transfers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, atVertexData, sizeof(float) * 16);
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[1024], atIndexData, sizeof(uint32_t) * 6);

    // begin recording
    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(&ptAppData->tGraphics, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder tEncoder = gptGfx->begin_blit_pass(&ptAppData->tGraphics, &tCommandBuffer);
    gptGfx->copy_buffer(&tEncoder, ptAppData->tStagingBuffer, ptAppData->tVertexBuffer, 0, 0, sizeof(float) * 16);
    gptGfx->copy_buffer(&tEncoder, ptAppData->tStagingBuffer, ptAppData->tIndexBuffer, 1024, 0, sizeof(uint32_t) * 6);
    gptGfx->end_blit_pass(&tEncoder);

    // finish recording
    gptGfx->end_command_recording(&ptAppData->tGraphics, &tCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer_blocking(&ptAppData->tGraphics, &tCommandBuffer, NULL);

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
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    ptAppData->tTexture = gptDevice->create_texture(ptDevice, &tTextureDesc, "texture");

    // retrieve new texture
    plTexture* ptTexture = gptDevice->get_texture(ptDevice, ptAppData->tTexture);

    // allocate memory
    const plDeviceMemoryAllocation tTextureAllocation = gptDevice->allocate_memory(ptDevice,
        ptTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        "texture memory");

    // bind memory
    gptDevice->bind_texture_to_memory(ptDevice, ptAppData->tTexture, &tTextureAllocation);

    // copy memory to mapped staging buffer
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pucImageData, iImageWidth * iImageHeight * 4);

    // begin recording
    tCommandBuffer = gptGfx->begin_command_recording(&ptAppData->tGraphics, NULL);

    // begin blit pass, copy data, end pass
    tEncoder = gptGfx->begin_blit_pass(&ptAppData->tGraphics, &tCommandBuffer);

    const plBufferImageCopy tBufferImageCopy = {
        .tImageExtent = {(uint32_t)iImageWidth, (uint32_t)iImageHeight, 1},
        .uLayerCount = 1
    };

    gptGfx->copy_buffer_to_texture(&tEncoder, ptAppData->tStagingBuffer, ptAppData->tTexture, 1, &tBufferImageCopy);
    gptGfx->end_blit_pass(&tEncoder);

    // finish recording
    gptGfx->end_command_recording(&ptAppData->tGraphics, &tCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer_blocking(&ptAppData->tGraphics, &tCommandBuffer, NULL);

    // free image data
    gptImage->free(pucImageData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~samplers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plSamplerDesc tSamplerDesc = {
        .tFilter         = PL_FILTER_LINEAR,
        .fMinMip         = 0.0f,
        .fMaxMip         = 1.0f,
        .tVerticalWrap   = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };
    ptAppData->tSampler = gptDevice->create_sampler(ptDevice, &tSamplerDesc, "sampler");

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
    ptAppData->tBindGroup0 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout, "bind group 0");

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
        .atTextures = &tTextureData
    };
    gptDevice->update_bind_group(ptDevice, ptAppData->tBindGroup0, &tBGData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plShaderDescription tShaderDesc = {
        .tVertexShader = gptShader->compile_glsl("../examples/shaders/example_6.vert", "main"),
        .tPixelShader = gptShader->compile_glsl("../examples/shaders/example_6.frag", "main"),
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
        .tRenderPassLayout = ptAppData->tGraphics.tMainRenderPassLayout,
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
    ptAppData->tShader = gptDevice->create_shader(ptDevice, &tShaderDesc);

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
    gptDevice->flush_device(&ptAppData->tGraphics.tDevice);
    gptDevice->destroy_shader(&ptAppData->tGraphics.tDevice, ptAppData->tShader);
    gptDevice->destroy_buffer(&ptAppData->tGraphics.tDevice, ptAppData->tVertexBuffer);
    gptDevice->destroy_buffer(&ptAppData->tGraphics.tDevice, ptAppData->tIndexBuffer);
    gptDevice->destroy_buffer(&ptAppData->tGraphics.tDevice, ptAppData->tStagingBuffer);
    gptDevice->destroy_texture(&ptAppData->tGraphics.tDevice, ptAppData->tTexture);
    gptGfx->cleanup(&ptAppData->tGraphics);
    gptWindows->destroy_window(ptAppData->ptWindow);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    // perform any operations required during a window resize
    gptGfx->resize(&ptAppData->tGraphics); // recreates swapchain
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    pl_begin_profile_frame();

    gptIO->new_frame();

    // for convience
    plGraphics* ptGraphics = &ptAppData->tGraphics;

    // begin new frame
    if(!gptGfx->begin_frame(ptGraphics))
    {
        gptGfx->resize(ptGraphics);
        pl_end_profile_frame();
        return;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~

    // expected timeline semaphore values
    uint64_t ulValue0 = ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex];
    uint64_t ulValue1 = ulValue0 + 1;
    ptAppData->aulNextTimelineValue[ptGraphics->uCurrentFrameIndex] = ulValue1;

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {ulValue0},
    };
    plCommandBuffer tCommandBuffer = gptGfx->begin_command_recording(ptGraphics, &tBeginInfo);

    // begin main renderpass (directly to swapchain)
    plRenderEncoder tEncoder = gptGfx->begin_render_pass(ptGraphics, &tCommandBuffer, ptGraphics->tMainRenderPass);

    // submit nonindexed draw using basic API
    gptGfx->bind_shader(&tEncoder, ptAppData->tShader);
    gptGfx->bind_vertex_buffer(&tEncoder, ptAppData->tVertexBuffer);

    // retrieve dynamic binding data
    plDynamicBinding tDynamicBinding = gptDevice->allocate_dynamic_data(&ptGraphics->tDevice, sizeof(plVec4));
    plVec4* tTintColor = (plVec4*)tDynamicBinding.pcData;
    tTintColor->r = 1.0f;
    tTintColor->g = 1.0f;
    tTintColor->b = 1.0f;
    tTintColor->a = 1.0f;

    // bind groups (up to 3 bindgroups + 1 dynamic binding are allowed)
    gptGfx->bind_graphics_bind_groups(&tEncoder, ptAppData->tShader, 0, 1, &ptAppData->tBindGroup0, &tDynamicBinding);

    const plDrawIndex tDraw = {
        .uInstanceCount = 1,
        .uIndexCount    = 6,
        .tIndexBuffer   = ptAppData->tIndexBuffer
    };
    gptGfx->draw_indexed(&tEncoder, 1, &tDraw);

    // end render pass
    gptGfx->end_render_pass(&tEncoder);

    // end recording
    gptGfx->end_command_recording(ptGraphics, &tCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {ptAppData->atSempahore[ptGraphics->uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {ulValue1},
    };

    if(!gptGfx->present(ptGraphics, &tCommandBuffer, &tSubmitInfo))
        gptGfx->resize(ptGraphics);

    pl_end_profile_frame();
}