/*
    Two native windows, one graphics device, and a shared triangle vertex buffer.
    Each window owns its surface, swapchain, and compatible pipeline. Resize or
    minimize either window independently. Closing the main window exits; closing
    the secondary window leaves the main window running.
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] internal api
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pl.h"
#include "pl_graphics_ext.h"
#include "pl_shader_ext.h"
#include "pl_platform_ext.h"
#include "pl_vfs_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plExampleViewport
{
    plWindow*      ptWindow;
    plSurface*     ptSurface;
    plSwapchain*   ptSwapchain;
    plShaderHandle tShader;
    bool           bResize;
    bool           bClose;
} plExampleViewport;

typedef struct _plAppData
{
    plExampleViewport atViewports[2];
    plDevice*         ptDevice;
    plCommandPool*    aptCommandPools[2];
    plBufferHandle    tVertexBuffer;
    float             fTime;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*       gptIO      = NULL;
const plWindowI*   gptWindows = NULL;
const plGraphicsI* gptGfx     = NULL;
const plShaderI*   gptShader  = NULL;
const plVfsI*      gptVfs     = NULL;
const plFileI*     gptFile    = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void
pl__example_window_event(const plWindowEvent* ptEvent, void* pUserData)
{
    plAppData* ptAppData = pUserData;
    for(uint32_t i = 0; i < 2; i++)
    {
        plExampleViewport* ptViewport = &ptAppData->atViewports[i];
        if(ptViewport->ptWindow != ptEvent->ptWindow)
            continue;

        if(ptEvent->tType == PL_WINDOW_EVENT_TYPE_WINDOW_CLOSE)
        {
            ptViewport->bClose = true;
            if(i == 0)
                gptIO->get_io()->bRunning = false;
        }
        else if(ptEvent->tType == PL_WINDOW_EVENT_TYPE_WINDOW_RESIZE)
        {
            ptViewport->bResize = true;
            // The runtime normally pauses when the main window is minimized.
            // This app instead checks each window, allowing the other to render.
            gptIO->get_io()->bViewportMinimized = false;
        }
    }
}

static plShaderHandle
pl__example_create_shader(plAppData* ptAppData, plExampleViewport* ptViewport)
{
    plShaderDesc tDesc = {
        .tVertexShader   = gptShader->load_glsl("example_gfx_0.vert", "main", NULL, NULL),
        .tFragmentShader = gptShader->load_glsl("example_gfx_0.frag", "main", NULL, NULL),
        .eMSAASampleCount = 1,
        .tGraphicsState = {
            .eDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .eCullMode           = PL_CULL_MODE_NONE,
            .eStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .uStencilRef         = 0xff,
            .uStencilMask        = 0xff,
            .eStencilOpFail      = PL_STENCIL_OP_KEEP,
            .eStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .eStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {{.atAttributes = {
            {.eFormat = PL_VERTEX_FORMAT_FLOAT2},
            {.eFormat = PL_VERTEX_FORMAT_FLOAT4}
        }}},
        .atBlendStates = {{.uColorWriteMask = PL_COLOR_WRITE_MASK_ALL}}
    };
    tDesc.tRenderAttachmentInfo.aeColorFormats[0] = gptGfx->get_swapchain_info(ptViewport->ptSwapchain).eFormat;
    return gptGfx->create_shader(ptAppData->ptDevice, &tDesc);
}

static void
pl__example_destroy_viewport(plAppData* ptAppData, plExampleViewport* ptViewport)
{
    if(!ptViewport->ptWindow)
        return;

    // The caller retires GPU work before destroying a viewport.
    gptWindows->set_callback(ptViewport->ptWindow, NULL, NULL);
    gptGfx->destroy_shader(ptAppData->ptDevice, ptViewport->tShader);
    gptGfx->cleanup_swapchain(ptViewport->ptSwapchain);
    gptGfx->cleanup_surface(ptViewport->ptSurface);
    gptWindows->destroy(ptViewport->ptWindow);
    memset(ptViewport, 0, sizeof(*ptViewport));
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    if(!ptAppData)
    {
        const plExtensionRegistryI* ptExtensions = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
        ptExtensions->load("pl_unity_ext", NULL, NULL, true);
        ptExtensions->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
        ptExtensions->load("pl_shader_ext", "pl_load_shader_ext", "pl_unload_shader_ext", true);
        ptExtensions->load("pl_graphics_ext", "pl_load_graphics_ext", "pl_unload_graphics_ext", true);
    }

    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptGfx     = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptShader  = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptVfs     = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptFile    = pl_get_api_latest(ptApiRegistry, plFileI);

    if(ptAppData)
    {
        // Callback addresses belong to the app DLL, so refresh them on reload.
        for(uint32_t i = 0; i < 2; i++)
        {
            if(ptAppData->atViewports[i].ptWindow)
                gptWindows->set_callback(ptAppData->atViewports[i].ptWindow, pl__example_window_event, ptAppData);
        }
        return ptAppData;
    }

    ptAppData = calloc(1, sizeof(plAppData));
    gptFile->create_directory("../cache");
    gptFile->create_directory("../cache/shaders");
    gptVfs->mount_directory("/shaders", "../shaders", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/cache", "../cache", PL_VFS_MOUNT_FLAGS_NONE);

    const plShaderOptions tOptions = {
        .apcIncludeDirectories = {"../examples/shaders/"},
        .apcDirectories = {"../examples/shaders/"},
        .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE
    };
    gptShader->initialize(&tOptions);
    const plGraphicsInit tGraphicsInit = {
        .uFramesInFlight = 2,
        .eFlags = PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED | PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED
    };
    gptGfx->initialize(&tGraphicsInit);

    for(uint32_t i = 0; i < 2; i++)
    {
        plExampleViewport* ptViewport = &ptAppData->atViewports[i];
        const plWindowDesc tDesc = {
            .pcTitle = i == 0 ? "Example GFX 7 - Main" : "Example GFX 7 - Secondary",
            .iXPos = 80 + (int)i * 660,
            .iYPos = 100,
            .uWidth = 640,
            .uHeight = 480,
            .tMode = PL_WINDOW_PRESENTATION_MODE_GRAPHICS_API
        };
        gptWindows->create(tDesc, &ptViewport->ptWindow);
        gptWindows->set_callback(ptViewport->ptWindow, pl__example_window_event, ptAppData);
        ptViewport->ptSurface = gptGfx->create_surface(ptViewport->ptWindow);
    }

    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    gptGfx->enumerate_devices(atDeviceInfos, &uDeviceCount);
    plDeviceInit tDeviceInit = {
        .ptSurface = ptAppData->atViewports[0].ptSurface,
        .szDynamicBufferBlockSize = 1024 * 1024
    };
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        if(atDeviceInfos[i].eType == PL_DEVICE_TYPE_DISCRETE)
        {
            tDeviceInit.uDeviceIdx = i;
            break;
        }
    }
    ptAppData->ptDevice = gptGfx->create_device(&tDeviceInit);

    for(uint32_t i = 0; i < 2; i++)
    {
        plExampleViewport* ptViewport = &ptAppData->atViewports[i];
        const plSwapchainInit tInit = {.bVSync = true, .uWidth = 640, .uHeight = 480, .eSampleCount = 1};
        ptViewport->ptSwapchain = gptGfx->create_swapchain(ptAppData->ptDevice, ptViewport->ptSurface, &tInit);
        ptViewport->tShader = pl__example_create_shader(ptAppData, ptViewport);
        ptAppData->aptCommandPools[i] = gptGfx->create_command_pool(ptAppData->ptDevice, &(plCommandPoolDesc){0});
        gptWindows->show(ptViewport->ptWindow);
    }

    const float afVertices[] = { // x, y, r, g, b, a
        -0.7f,  0.7f, 1.0f, 0.2f, 0.1f, 1.0f,
         0.7f,  0.7f, 0.1f, 1.0f, 0.3f, 1.0f,
         0.0f, -0.7f, 0.2f, 0.4f, 1.0f, 1.0f
    };
    const plBufferDesc tBufferDesc = {
        .eUsage = PL_BUFFER_USAGE_VERTEX,
        .szByteSize = sizeof(afVertices),
        .pcDebugName = "shared viewport triangle"
    };
    ptAppData->tVertexBuffer = gptGfx->create_buffer(ptAppData->ptDevice, &tBufferDesc, NULL);
    plBuffer* ptBuffer = gptGfx->get_buffer(ptAppData->ptDevice, ptAppData->tVertexBuffer);
    plDeviceMemoryAllocation tMemory = gptGfx->allocate_memory(ptAppData->ptDevice, ptBuffer->tMemoryRequirements.ulSize, PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT, ptBuffer->tMemoryRequirements.uMemoryTypeBits, "shared viewport triangle");
    gptGfx->bind_buffer_to_memory(ptAppData->ptDevice, ptAppData->tVertexBuffer, &tMemory);
    // Metal exposes the mapped address on the buffer after binding the heap.
    memcpy(ptBuffer->tMemoryAllocation.pHostMapped, afVertices, sizeof(afVertices));
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptGfx->flush_device(ptAppData->ptDevice);
    for(uint32_t i = 0; i < 2; i++)
    {
        pl__example_destroy_viewport(ptAppData, &ptAppData->atViewports[i]);
        gptGfx->cleanup_command_pool(ptAppData->aptCommandPools[i]);
    }
    gptGfx->destroy_buffer(ptAppData->ptDevice, ptAppData->tVertexBuffer);
    gptGfx->cleanup_device(ptAppData->ptDevice);
    gptGfx->cleanup();
    gptShader->cleanup();
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    const plWindowEvent tEvent = {.tType = PL_WINDOW_EVENT_TYPE_WINDOW_RESIZE, .ptWindow = ptWindow};
    pl__example_window_event(&tEvent, ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    bool abVisible[2] = {false, false};
    ptAppData->fTime += gptIO->get_io()->fDeltaTime;

    // Retire and recreate resources BEFORE acquiring any images this frame.
    for(uint32_t i = 0; i < 2; i++)
    {
        plExampleViewport* ptViewport = &ptAppData->atViewports[i];
        if(!ptViewport->ptWindow)
            continue;
        if(ptViewport->bClose)
        {
            gptGfx->flush_device(ptAppData->ptDevice);
            pl__example_destroy_viewport(ptAppData, ptViewport);
            continue;
        }

        plWindowAttributeValue tMinimized = {0};
        if(gptWindows->get_attribute(ptViewport->ptWindow, PL_WINDOW_ATTRIBUTE_MINIMIZED, &tMinimized) && tMinimized.bValue)
            continue;

        plWindowAttributeValue tSize = {0};
        const bool bHasSize = gptWindows->get_attribute(ptViewport->ptWindow, PL_WINDOW_ATTRIBUTE_SIZE, &tSize);
        if(bHasSize && (tSize.tuVec2.x == 0 || tSize.tuVec2.y == 0))
            continue;

        abVisible[i] = true;
        if(ptViewport->bResize)
        {
            gptGfx->flush_device(ptAppData->ptDevice);
            const plSwapchainInfo tOldInfo = gptGfx->get_swapchain_info(ptViewport->ptSwapchain);
            const plSwapchainInit tInit = {
                .bVSync = true,
                .uWidth = bHasSize ? tSize.tuVec2.x : tOldInfo.uWidth,
                .uHeight = bHasSize ? tSize.tuVec2.y : tOldInfo.uHeight,
                .eSampleCount = 1
            };
            gptGfx->recreate_swapchain(ptViewport->ptSwapchain, &tInit);
            if(gptGfx->get_swapchain_info(ptViewport->ptSwapchain).eFormat != tOldInfo.eFormat)
            {
                gptGfx->destroy_shader(ptAppData->ptDevice, ptViewport->tShader);
                ptViewport->tShader = pl__example_create_shader(ptAppData, ptViewport);
            }
            ptViewport->bResize = false;
        }
    }

    gptGfx->begin_frame(ptAppData->ptDevice);
    plCommandPool* ptPool = ptAppData->aptCommandPools[gptGfx->get_current_frame_index()];
    gptGfx->reset_command_pool(ptPool, 0);
    plCommandBuffer* ptCmd = gptGfx->request_command_buffer(ptPool, "two viewports");
    gptGfx->begin_command_recording(ptCmd);
    plSwapchain* aptPresent[2] = {0};
    uint32_t uPresentCount = 0;

    for(uint32_t i = 0; i < 2; i++)
    {
        if(!abVisible[i])
            continue;

        plExampleViewport* ptViewport = &ptAppData->atViewports[i];
        if(!gptGfx->acquire_swapchain_image(ptViewport->ptSwapchain))
        {
            ptViewport->bResize = true;
            continue; // Already acquired windows still need to be presented.
        }
        aptPresent[uPresentCount++] = ptViewport->ptSwapchain;
        const plSwapchainInfo tInfo = gptGfx->get_swapchain_info(ptViewport->ptSwapchain);
        const uint32_t uImage = gptGfx->get_current_swapchain_image_index(ptViewport->ptSwapchain);
        const float fWidth = (float)tInfo.uWidth;
        const float fHeight = (float)tInfo.uHeight;
        plRenderInfo tRenderInfo = {
            .tRenderArea = {{0, 0}, {fWidth, fHeight}},
            .atColorAttachments = {{
                .tTexture = gptGfx->get_swapchain_images(ptViewport->ptSwapchain, NULL)[uImage],
                .eLoadOp = PL_LOAD_OP_CLEAR,
                .eStoreOp = PL_STORE_OP_STORE,
                .eUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .tClearColor = {i == 0 ? 0.025f : 0.10f, 0.04f, i == 0 ? 0.10f : 0.025f, 1.0f}
            }}
        };
        gptGfx->begin_render_pass(ptCmd, &tRenderInfo, NULL);
        const float fSize = fminf(fWidth, fHeight) * (0.7f + 0.1f * sinf(ptAppData->fTime + (float)i * 2.0f));
        const plRenderViewport tRenderViewport = {
            .fX = (fWidth - fSize) * 0.5f,
            .fY = (fHeight - fSize) * 0.5f,
            .fWidth = fSize,
            .fHeight = fSize,
            .fMaxDepth = 1.0f
        };
        const plScissor tScissor = {.uWidth = tInfo.uWidth, .uHeight = tInfo.uHeight};
        gptGfx->set_viewport(ptCmd, &tRenderViewport);
        gptGfx->set_scissor_region(ptCmd, &tScissor);
        gptGfx->bind_shader(ptCmd, ptViewport->tShader);
        gptGfx->bind_vertex_buffer(ptCmd, ptAppData->tVertexBuffer);
        const plDraw tDraw = {.uInstanceCount = 1, .uVertexCount = 3};
        gptGfx->draw(ptCmd, 1, &tDraw);
        gptGfx->end_render_pass(ptCmd);
    }

    gptGfx->end_command_recording(ptCmd);
    // One submission presents the acquired subset (including zero windows).
    if(!gptGfx->present(ptCmd, NULL, aptPresent, uPresentCount))
    {
        for(uint32_t i = 0; i < 2; i++)
        {
            if(ptAppData->atViewports[i].ptWindow)
                ptAppData->atViewports[i].bResize = true;
        }
    }
    gptGfx->return_command_buffer(ptCmd);
}
