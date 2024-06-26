/*
   example_4.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates vertex buffers
     - demonstrates graphics shaders
     - demonstrates non-index drawing
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
    plBufferHandle tVertexBuffer;

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

    // load graphics extension (provides graphics & device apis)
    ptExtensionRegistry->load("pl_graphics_ext", NULL, NULL, false);
    ptExtensionRegistry->load("pl_shader_ext", NULL, NULL, false);
    
    // load required apis (NULL if not available)
    gptIO      = ptApiRegistry->first(PL_API_IO);
    gptWindows = ptApiRegistry->first(PL_API_WINDOW);
    gptGfx     = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice  = ptApiRegistry->first(PL_API_DEVICE);
    gptShader  = ptApiRegistry->first(PL_API_SHADER);

    // use window API to create a window
    const plWindowDesc tWindowDesc = {
        .pcName  = "Example 4",
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // vertex buffer data
    const float atVertexData[] = { // x, y, r, g, b, a
        -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
         0.0f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f
    };

    // create vertex buffer
    const plBufferDescription tBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX,
        .uByteSize = sizeof(float) * 18
    };
    ptAppData->tVertexBuffer = gptDevice->create_buffer(ptDevice, &tBufferDesc, "vertex buffer");

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptVertexBuffer = gptDevice->get_buffer(ptDevice, ptAppData->tVertexBuffer);

    // allocate memory for the vertex buffer
    // NOTE: for this example we are using host visible memory for simplicity (PL_MEMORY_GPU_CPU)
    //       which is persistently mapped. For rarely updated memory, device local memory should
    //       be used, with uploads transfered from staging buffers (see example 5)
    const plDeviceMemoryAllocation tAllocation = gptDevice->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU_CPU,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "vertex buffer memory");

    // bind the buffer to the new memory allocation
    gptDevice->bind_buffer_to_memory(ptDevice, ptAppData->tVertexBuffer, &tAllocation);

    // copy vertex data to newly allocated memory
    memcpy(ptVertexBuffer->tMemoryAllocation.pHostMapped, atVertexData, sizeof(float) * 18);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plShaderDescription tShaderDesc = {
        .tVertexShader = gptShader->compile_glsl("../examples/shaders/example_4.vert", "main"),
        .tPixelShader = gptShader->compile_glsl("../examples/shaders/example_4.frag", "main"),
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
        .tRenderPassLayout = ptAppData->tGraphics.tMainRenderPassLayout,
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

    const plDraw tDraw = {
        .uInstanceCount = 1,
        .uVertexCount   = 3
    };
    gptGfx->draw(&tEncoder, 1, &tDraw);

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