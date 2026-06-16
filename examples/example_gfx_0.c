/*
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates vertex buffers
     - demonstrates shaders
     - demonstrates non-index drawing
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*

    WARNING:

    The purpose of the graphics extension is NOT to make low level graphics
    programming easier.
    
    The purpose of the graphics extension is NOT to be an abstraction for the
    sake of abstraction.

    The graphics extension does not hold your hand. You are expected to be
    familar with either Vulkan or Metal 3.0 concepts.

    These examples mostly assume you understand low level graphics and will
    not attempt to explain those concepts (i.e. what is a vertex buffer?)

    BACKGROUND:

    The graphics extension is meant to be an extremely lightweight abstraction
    over the "modern" explicit graphics APIs (Vulkan/DirectX 12/Metal 4.0).
    Ideally it should be 1 to 1 when possible. The explicit control provided
    by these APIs are their power, so the extension tries to preserve that
    as much as possible while also allowing a graphics programmer the ability
    to write cross platform graphics code without too much consideration of
    the differences between the APIs. This is accomplished by careful
    consideration of the APIs and their common concepts and features. In some
    cases, the API is required to be stricter than the underlying API. For
    example, Vulkan makes it easy to issue draw calls and compute dispatches
    directly to command buffers while Metal requires these to be submitted
    to different "encoders" which can't be recording simutaneously. So the
    graphics extension introduces the concept of encoder to match the stricter
    API at the cost of some freedom Vulkan would normally allow. There is only
    a few cases like this but we will note them in these examples.

    EXAMPLE:
    
    This example introduces the graphics and shader extensions. It will also
    utilize the starter extension introduced in the "basic" examples. Yes, one
    of the starter extension's main goals is to help with graphics extension
    but for learning purposes it will help to narrow the scope of each example
    since the graphics API contains alot of boilerplate and is very verbose.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <stdio.h>
#include <string.h> // memset
#include "pl.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_shader_ext.h"
#include "pl_starter_ext.h"

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
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;
const plShaderI*   gptShader  = NULL;
const plStarterI*  gptStarter = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "ptAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptGfx     = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptShader  = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptStarter = pl_get_api_latest(ptApiRegistry, plStarterI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx     = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptShader  = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptStarter = pl_get_api_latest(ptApiRegistry, plStarterI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 0",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .eFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we will remove this flag so we can handle
    // management of the shader extension
    tStarterInit.eFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);
    
    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../examples/shaders/"
        },
        .apcDirectories = {
            "../shaders/",
            "../examples/shaders/"
        },
        .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // for convience
    plDevice* ptDevice = gptStarter->get_device();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~buffers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // vertex buffer data
    const float afVertexData[] = { // x, y, r, g, b, a
        -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
         0.0f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f
    };

    // create vertex buffer
    const plBufferDesc tBufferDesc = {
        .eUsage       = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize   = sizeof(float) * PL_ARRAYSIZE(afVertexData),
        .pcDebugName  = "vertex buffer"
    };
    gptStarter->create_buffer(&tBufferDesc, afVertexData, &ptAppData->tVertexBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plShaderDesc tShaderDesc = {
        .tVertexShader    = gptShader->load_glsl("example_gfx_0.vert", "main", NULL, NULL),
        .tFragmentShader  = gptShader->load_glsl("example_gfx_0.frag", "main", NULL, NULL),
        .tGraphicsState = {
            .bDepthWriteEnabled  = 0,
            .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .eCullMode           = PL_CULL_MODE_NONE,
            .bWireframe          = 0,
            .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .uStencilRef         = 0xff,
            .uStencilMask        = 0xff,
            .eStencilOpFail      = PL_STENCIL_OP_KEEP,
            .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .eStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .atAttributes = {
                    {.eFormat = PL_VERTEX_FORMAT_FLOAT2 },
                    {.eFormat = PL_VERTEX_FORMAT_FLOAT4 },
                }
            }
        },
        .atBlendStates = {
            { .bBlendEnabled   = false, .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL }
        }
    };
    gptStarter->get_render_attachment_info(&tShaderDesc.tRenderAttachmentInfo);
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
    plDevice* ptDevice = gptStarter->get_device();

    // ensure the GPU is done with our resources
    gptGfx->flush_device(ptDevice);

    // cleanup our resources
    gptGfx->destroy_buffer(ptDevice, ptAppData->tVertexBuffer);

    gptShader->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    // start main pass & return the command buffer being used
    plCommandBuffer* ptCmdBuffer = gptStarter->begin_main_pass();

    // submit nonindexed draw using basic API
    gptGfx->bind_shader(ptCmdBuffer, ptAppData->tShader);
    gptGfx->bind_vertex_buffer(ptCmdBuffer, ptAppData->tVertexBuffer);

    plDraw tDraw = {
        .uInstanceCount = 1,
        .uVertexCount   = 3
    };
    gptGfx->draw(ptCmdBuffer, 1, &tDraw);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}
