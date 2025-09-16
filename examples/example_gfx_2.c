/*
   example_gfx_2.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates bind groups
     - demonstrates vertex, index, staging buffers
     - demonstrates samplers, textures, bind groups
     - demonstrates shaders
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
#include "pl.h"
#include "pl_ds.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_log_ext.h"
#include "pl_platform_ext.h"
#include "pl_graphics_ext.h"
#include "pl_image_ext.h"
#include "pl_shader_ext.h"
#include "pl_starter_ext.h"
#include "pl_vfs_ext.h"

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
    plBindGroupHandle       tBindGroup0;
    plBindGroupLayoutHandle tBindGroupLayout0;

    // graphics & sync objects
    plBindGroupPool* ptBindGroupPool;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;
const plImageI*    gptImage   = NULL;
const plShaderI*   gptShader  = NULL;
const plFileI*     gptFile    = NULL;
const plStarterI*  gptStarter = NULL;
const plVfsI*      gptVfs     = NULL;

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
        gptImage   = pl_get_api_latest(ptApiRegistry, plImageI);
        gptFile    = pl_get_api_latest(ptApiRegistry, plFileI);
        gptStarter = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptVfs     = pl_get_api_latest(ptApiRegistry, plVfsI);

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
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx     = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptShader  = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptImage   = pl_get_api_latest(ptApiRegistry, plImageI);
    gptFile    = pl_get_api_latest(ptApiRegistry, plFileI);
    gptStarter = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptVfs     = pl_get_api_latest(ptApiRegistry, plVfsI);

    gptVfs->mount_directory("/assets", "../data/pilotlight-assets-master", PL_VFS_MOUNT_FLAGS_NONE);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 2",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we will remove this flag so we can handle
    // management of the shader extension
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    plDevice* ptDevice = gptStarter->get_device();

    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../examples/shaders/"
        },
        .apcDirectories = {
            "../shaders/",
            "../examples/shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

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
        .szByteSize  = sizeof(float) * PL_ARRAYSIZE(atVertexData),
        .pcDebugName = "vertex buffer"
    };
    ptAppData->tVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tVertexBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tVertexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
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
        .szByteSize  = sizeof(uint32_t) * PL_ARRAYSIZE(atIndexData),
        .pcDebugName = "index buffer"
    };
    ptAppData->tIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tIndexBuffer);

    // allocate memory for the index buffer
    const plDeviceMemoryAllocation tIndexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "index buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tIndexBuffer, &tIndexBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~staging buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create vertex buffer
    const plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_STAGING,
        .szByteSize  = 1280000,
        .pcDebugName = "staging buffer"
    };
    ptAppData->tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tStagingBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tStagingBuffer, &tStagingBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~transfers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, atVertexData, sizeof(float) * PL_ARRAYSIZE(atVertexData));
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[1024], atIndexData, sizeof(uint32_t) * PL_ARRAYSIZE(atIndexData));

    plBlitEncoder* ptEncoder = gptStarter->get_blit_encoder();
    gptGfx->copy_buffer(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tVertexBuffer, 0, 0, sizeof(float) * PL_ARRAYSIZE(atVertexData));
    gptGfx->copy_buffer(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tIndexBuffer, 1024, 0, sizeof(uint32_t) * PL_ARRAYSIZE(atIndexData));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // load image file from disk
    size_t szImageFileSize = gptVfs->get_file_size_str("/assets/textures/SpriteMapExample.png");
    plVfsFileHandle tSpriteSheet = gptVfs->open_file("/assets/textures/SpriteMapExample.png", PL_VFS_FILE_MODE_READ);
    gptVfs->read_file(tSpriteSheet, NULL, &szImageFileSize);
    unsigned char* pucBuffer = malloc(szImageFileSize);
    gptVfs->read_file(tSpriteSheet, pucBuffer, &szImageFileSize);
    gptVfs->close_file(tSpriteSheet);

    // load actual data from file data
    int iImageWidth = 0;
    int iImageHeight = 0;
    int _unused;
    unsigned char* pucImageData = gptImage->load(pucBuffer, (int)szImageFileSize, &iImageWidth, &iImageHeight, &_unused, 4);
    free(pucBuffer);

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
    ptAppData->tTexture = gptGfx->create_texture(ptDevice, &tTextureDesc, NULL);

    // retrieve new texture (also could have used out param from create_texture above)
    plTexture* ptTexture = gptGfx->get_texture(ptDevice, ptAppData->tTexture);

    // allocate memory
    const plDeviceMemoryAllocation tTextureAllocation = gptGfx->allocate_memory(ptDevice,
        ptTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        "texture memory");

    // bind memory
    gptGfx->bind_texture_to_memory(ptDevice, ptAppData->tTexture, &tTextureAllocation);

    // set the initial texture usage (this is a no-op in metal but does layout transition for vulkan)
    gptGfx->set_texture_usage(ptEncoder, ptAppData->tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);

    // copy memory to mapped staging buffer
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[2048], pucImageData, iImageWidth * iImageHeight * 4);

    const plBufferImageCopy tBufferImageCopy = {
        .uImageWidth    = (uint32_t)iImageWidth,
        .uImageHeight   = (uint32_t)iImageHeight,
        .uImageDepth    = 1,
        .uLayerCount    = 1,
        .szBufferOffset = 2048
    };

    gptGfx->copy_buffer_to_texture(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tTexture, 1, &tBufferImageCopy);

    gptStarter->return_blit_encoder(ptEncoder);

    // free image data
    gptImage->free(pucImageData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~samplers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plSamplerDesc tSamplerDesc = {
        .tMagFilter    = PL_FILTER_LINEAR,
        .tMinFilter    = PL_FILTER_LINEAR,
        .fMinMip       = 0.0f,
        .fMaxMip       = 1.0f,
        .tVAddressMode = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .tUAddressMode = PL_ADDRESS_MODE_CLAMP_TO_EDGE,
        .pcDebugName   = "sampler"
    };
    ptAppData->tSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // NOTE: Bind group pools map directly to descriptor pools in Vulkan. In Metal
    //       a bind group is just an argument buffer managed as a pool

    // create bind group pool
    const plBindGroupPoolDesc tBindGroupPoolDesc = {
        .tFlags                      = PL_BIND_GROUP_POOL_FLAGS_NONE,
        .szSamplerBindings           = 1,
        .szSampledTextureBindings    = 1
    };
    ptAppData->ptBindGroupPool = gptGfx->create_bind_group_pool(ptDevice, &tBindGroupPoolDesc);

    // NOTE: Bind group layouts and bind groups map directly to Vulkan descriptor
    //       set layouts and descriptors. The metal backend accomplishes the same
    //       concept but treats bind groups as simple offsets into argument buffers.

    // create bind group
    const plBindGroupLayoutDesc tBindGroupLayout = {
        .atSamplerBindings = {
            { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
        },
        .atTextureBindings = {
            {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        }
    };
    ptAppData->tBindGroupLayout0 = gptGfx->create_bind_group_layout(ptDevice, &tBindGroupLayout);

    const plBindGroupDesc tBindGroupDesc = {
        .tLayout     = ptAppData->tBindGroupLayout0,
        .pcDebugName = "bind group 0",
        .ptPool      = ptAppData->ptBindGroupPool
    };
    ptAppData->tBindGroup0 = gptGfx->create_bind_group(ptDevice, &tBindGroupDesc);

    // update bind group (actually point bind groups to GPU resources)
    const plBindGroupUpdateSamplerData tSamplerData = {
        .tSampler = ptAppData->tSampler,
        .uSlot    = 0
    };

    const plBindGroupUpdateTextureData tTextureData = {
        .tTexture = ptAppData->tTexture,
        .uSlot    = 1,
        .tType    = PL_TEXTURE_BINDING_TYPE_SAMPLED
    };

    const plBindGroupUpdateData tBGData = {
        .uSamplerCount     = 1,
        .atSamplerBindings = &tSamplerData,
        .uTextureCount     = 1,
        .atTextureBindings = &tTextureData
    };
    gptGfx->update_bind_group(ptDevice, ptAppData->tBindGroup0, &tBGData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    const plShaderDesc tShaderDesc = {
        .tVertexShader    = gptShader->load_glsl("example_gfx_2.vert", "main", NULL, NULL),
        .tFragmentShader  = gptShader->load_glsl("example_gfx_2.frag", "main", NULL, NULL),
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
        .atVertexBufferLayouts = {
            {
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT2},
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT2},
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled = false
            }
        },
        .tRenderPassLayout = gptStarter->get_render_pass_layout(),
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    { .uSlot = 0, .tStages = PL_SHADER_STAGE_FRAGMENT}
                },
                .atTextureBindings = {
                    {.uSlot = 1, .tStages = PL_SHADER_STAGE_FRAGMENT, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
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
    plDevice* ptDevice = gptStarter->get_device();

    // ensure the GPU is done with our resources
    gptGfx->flush_device(ptDevice);

    // cleanup our resources
    gptGfx->destroy_buffer(ptDevice, ptAppData->tVertexBuffer);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tIndexBuffer);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tStagingBuffer);
    gptGfx->destroy_texture(ptDevice, ptAppData->tTexture);
    gptGfx->cleanup_bind_group_pool(ptAppData->ptBindGroupPool);

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

    plDevice* ptDevice = gptStarter->get_device();

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    // submit nonindexed draw using basic API
    gptGfx->bind_shader(ptEncoder, ptAppData->tShader);
    gptGfx->bind_vertex_buffer(ptEncoder, ptAppData->tVertexBuffer);

    // retrieve dynamic binding data
    // NOTE: This system is meant frequently updated shader data. Underneath its just simple
    //       bump allocator (very fast).
    plDynamicDataBlock tCurrentDynamicBufferBlock = gptGfx->allocate_dynamic_data_block(ptDevice);
    plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &tCurrentDynamicBufferBlock);
    plVec4* tTintColor = (plVec4*)tDynamicBinding.pcData;
    tTintColor->r = 1.0f;
    tTintColor->g = 1.0f;
    tTintColor->b = 1.0f;
    tTintColor->a = 1.0f;

    // bind bind groups (up to 3 bindgroups + 1 dynamic binding are allowed)
    // NOTE: dynamic bind groups are always bound to set 3 in the shader
    gptGfx->bind_graphics_bind_groups(ptEncoder, ptAppData->tShader, 0, 1, &ptAppData->tBindGroup0, 1, &tDynamicBinding);

    const plDrawIndex tDraw = {
        .uInstanceCount = 1,
        .uIndexCount    = 6,
        .tIndexBuffer   = ptAppData->tIndexBuffer
    };
    gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}
