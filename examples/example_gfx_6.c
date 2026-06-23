/*
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates bind groups
     - demonstrates textures, bind groups
     - demonstrates compute shaders
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
#include "pl_draw_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // shaders
    plComputeShaderHandle tHorizontalShader;
    plComputeShaderHandle tVerticalShader;

    // textures
    plTextureHandle tOriginalTexture;
    plTextureHandle tWorkingTexture;

    // texture draw handles
    plBindGroupHandle tOriginalTextureDrawHandle;
    plBindGroupHandle tWorkingTextureDrawHandle;

    // bind groups
    plBindGroupHandle       tBindGroup0;
    plBindGroupLayoutHandle tBindGroupLayout0;

    // bind pool
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
const plDrawI*     gptDraw    = NULL;

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
        gptDraw    = pl_get_api_latest(ptApiRegistry, plDrawI);

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
    gptImage   = pl_get_api_latest(ptApiRegistry, plImageI);
    gptFile    = pl_get_api_latest(ptApiRegistry, plFileI);
    gptStarter = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptDraw    = pl_get_api_latest(ptApiRegistry, plDrawI);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 6",
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
        .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_ALWAYS_COMPILE
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~textures~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // load actual data from file data
    int iImageWidth = 0;
    int iImageHeight = 0;
    int _unused;
    unsigned char* pucImageData = gptImage->load_from_file("../assets/core/textures/sprite_map.png", &iImageWidth, &iImageHeight, &_unused, 4);

    // create textures
    const plTextureDesc tOriginalTextureDesc = {
        .tDimensions = { (float)iImageWidth, (float)iImageHeight, 1},
        .eFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers     = 1,
        .uMips       = 1,
        .eType       = PL_TEXTURE_TYPE_2D,
        .eUsage      = PL_TEXTURE_USAGE_STORAGE | PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName = "texture"
    };

    // input texture
    gptStarter->create_texture(&tOriginalTextureDesc, pucImageData, iImageWidth * iImageHeight * 4, &ptAppData->tOriginalTexture);
    ptAppData->tOriginalTextureDrawHandle = gptDraw->create_bind_group_for_texture(ptAppData->tOriginalTexture);

    // output texture
    gptStarter->create_texture(&tOriginalTextureDesc, NULL, 0, &ptAppData->tWorkingTexture);
    ptAppData->tWorkingTextureDrawHandle = gptDraw->create_bind_group_for_texture(ptAppData->tWorkingTexture);
    
    // free image data
    gptImage->free(pucImageData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~bind groups~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // NOTE: Bind group pools map directly to descriptor pools in Vulkan. In Metal
    //       a bind group is just an argument buffer managed as a pool

    // create bind group pool
    const plBindGroupPoolDesc tBindGroupPoolDesc = {
        .eFlags                   = PL_BIND_GROUP_POOL_FLAGS_NONE,
        .szStorageTextureBindings = 2
    };
    ptAppData->ptBindGroupPool = gptGfx->create_bind_group_pool(ptDevice, &tBindGroupPoolDesc);

    // NOTE: Bind group layouts and bind groups map directly to Vulkan descriptor
    //       set layouts and descriptors. The metal backend accomplishes the same
    //       concept but treats bind groups as simple offsets into argument buffers.

    // create bind group
    const plBindGroupLayoutDesc tBindGroupLayout = {
        .atTextureBindings = {
            {.uSlot = 0, .eStages = PL_SHADER_STAGE_COMPUTE, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE},
            {.uSlot = 1, .eStages = PL_SHADER_STAGE_COMPUTE, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE}
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
    const plBindGroupUpdateData tBGData = {
        .atTextureBindings = {
            {.uSlot = 0, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptAppData->tOriginalTexture},
            {.uSlot = 1, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE, .tTexture = ptAppData->tWorkingTexture}
        }
    };
    gptGfx->update_bind_group(ptDevice, ptAppData->tBindGroup0, &tBGData);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plComputeShaderDesc tShaderDesc = {
        .pcDebugName = "gaussian_blur",
        .tShader = gptShader->load_glsl("example_gfx_6.comp", "main", NULL, NULL),
        .atBindGroupLayouts = {
            {
                .atTextureBindings = {
                    { .uSlot = 0, .eStages = PL_SHADER_STAGE_COMPUTE, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE },
                    { .uSlot = 1, .eStages = PL_SHADER_STAGE_COMPUTE, .eType = PL_TEXTURE_BINDING_TYPE_STORAGE }
                }
            }
        },
        .atConstants = {
            {
                .eType = PL_DATA_TYPE_INT,
                .uID = 0,
                .uOffset = 0
            }
        }
    };

    // vertical blur
    int iConstantData = 0;
    tShaderDesc.pTempConstantData = &iConstantData;
    ptAppData->tVerticalShader = gptGfx->create_compute_shader(ptDevice, &tShaderDesc);

    // horizontal blur
    iConstantData = 1;
    ptAppData->tHorizontalShader = gptGfx->create_compute_shader(ptDevice, &tShaderDesc);

    // begin recording command buffer
    plCommandBuffer* ptCommandBuffer = gptStarter->get_temporary_command_buffer();

    // must declare resources & how they are used
    plPassResources tPassResources = {
        .atTextures = {
            {
                .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ,
                .eStages = PL_PIPELINE_STAGE_COMPUTE,
                .tHandle = ptAppData->tOriginalTexture
            },
            {
                .eUsage  = PL_TEXTURE_USAGE_STORAGE,
                .eAccess = PL_PASS_RESOURCE_ACCESS_READ_WRITE,
                .eStages = PL_PIPELINE_STAGE_COMPUTE,
                .tHandle = ptAppData->tWorkingTexture
            }
        }
    };

    // begin compute pass
    gptGfx->begin_compute_pass(ptCommandBuffer, &tPassResources);

    plDispatch tDispatch = {
        .uGroupCountX = iImageWidth / 8,
        .uGroupCountY = iImageHeight / 8,
        .uGroupCountZ = 1,
        .uThreadPerGroupX = 8,
        .uThreadPerGroupY = 8,
        .uThreadPerGroupZ = 8
    };

    // vertical blur
    gptGfx->bind_compute_shader(ptCommandBuffer, ptAppData->tVerticalShader);
    gptGfx->bind_compute_bind_groups(ptCommandBuffer, ptAppData->tVerticalShader, 0, 1, &ptAppData->tBindGroup0, 0, NULL);
    gptGfx->dispatch(ptCommandBuffer, 1, &tDispatch);

    // horizontal blur
    gptGfx->bind_compute_shader(ptCommandBuffer, ptAppData->tHorizontalShader);
    gptGfx->bind_compute_bind_groups(ptCommandBuffer, ptAppData->tHorizontalShader, 0, 1, &ptAppData->tBindGroup0, 0, NULL);
    gptGfx->dispatch(ptCommandBuffer, 1, &tDispatch);

    // end compute pass
    gptGfx->end_compute_pass(ptCommandBuffer);

    // end recording, submit, & wait
    gptStarter->submit_temporary_command_buffer(ptCommandBuffer);

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
    gptGfx->destroy_texture(ptDevice, ptAppData->tOriginalTexture);
    gptGfx->destroy_texture(ptDevice, ptAppData->tWorkingTexture);
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

    plDrawLayer2D* ptLayer = gptStarter->get_background_layer();

    gptDraw->add_text(ptLayer, (plVec2){180.0f, 10.0f}, "Original", (plDrawTextOptions){.uColor = PL_COLOR_32_WHITE, .ptFont = gptStarter->get_default_font()});
    gptDraw->add_text(ptLayer, (plVec2){616.0f, 10.0f}, "Blurred", (plDrawTextOptions){.uColor = PL_COLOR_32_WHITE, .ptFont = gptStarter->get_default_font()});

    gptDraw->add_image(ptLayer, ptAppData->tOriginalTextureDrawHandle.uData,
        (plVec2){10.0f, 30.0f},
        (plVec2){426.0f, 414.0f}
    );

    gptDraw->add_image(ptLayer, ptAppData->tWorkingTextureDrawHandle.uData,
        (plVec2){436.0f, 30.0f},
        (plVec2){852.0f, 414.0f}
    );

    gptStarter->end_frame(); 
}
