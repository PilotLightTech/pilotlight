/*
   pl_starter_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal forward declarations
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] helper implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include "pl.h"
#include "pl_starter_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_profile_ext.h"
#include "pl_stats_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_shader_ext.h"
#include "pl_tools_ext.h"
#include "pl_gpu_allocators_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    static const plDataRegistryI*  gptDataRegistry  = NULL;
    static const plGraphicsI*      gptGfx           = NULL;
    static const plScreenLogI*     gptScreenLog     = NULL;
    static const plUiI*            gptUI            = NULL;
    static const plStatsI*         gptStats         = NULL;
    static const plDrawI*          gptDraw          = NULL;
    static const plIOI*            gptIOI           = NULL;
    static const plShaderI*        gptShader        = NULL;
    static const plProfileI*       gptProfile       = NULL;
    static const plConsoleI*       gptConsole       = NULL;
    static const plToolsI*         gptTools         = NULL;
    static const plGPUAllocatorsI* gptGpuAllocators = NULL;
    
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plStarterContext plStarterContext;

// helpers
static void pl__starter_create_render_pass(void);
static void pl__starter_activate_msaa(void);
static void pl__starter_deactivate_msaa(void);
static void pl__starter_activate_depth_buffer(void);
static void pl__starter_deactivate_depth_buffer(void);

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plStarterContext
{
    plStarterFlags           eFlags;
    plWindow*                ptWindow;
    plDevice*                ptDevice;
    plSwapchain*             ptSwapchain;
    plSurface*               ptSurface;
    plFont*                  ptDefaultFont;
    plTimelineSemaphore*     aptSemaphores[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t                 aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plCommandPool*           atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];
    // plRenderPassHandle       tRenderPass;
    // plRenderPassLayoutHandle tRenderPassLayout;
    plTextureHandle          tDepthTexture;
    plTextureHandle          tResolveTexture;
    bool                     bMainPassExplicit;
    bool                     bMSAAActivateRequested;
    bool                     bMSAADeactivateRequested;
    bool                     bDepthBufferActivateRequested;
    bool                     bDepthBufferDeactivateRequested;
    bool                     bVSyncChangeRequested;

    // drawing
    plDrawList2D*  ptFGDrawlist;
    plDrawList2D*  ptBGDrawlist;
    plDrawLayer2D* ptFGLayer;
    plDrawLayer2D* ptBGLayer;

    // current frame
    plCommandBuffer* ptCurrentCommandBuffer;

    // gpu allocators
    plDeviceMemoryAllocatorI* ptLocalDedicatedAllocator;
    plDeviceMemoryAllocatorI* ptLocalBuddyAllocator;
} plStarterContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plStarterContext* gptStarterCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_starter_initialize(plStarterInit tInit)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    gptStarterCtx->eFlags = tInit.eFlags;

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_CONSOLE_EXT)
        gptConsole->initialize((plConsoleSettings){.eFlags = PL_CONSOLE_FLAGS_POPUP});

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_SHADER_EXT)
    {
        static const plShaderOptions tDefaultShaderOptions = {
            .apcIncludeDirectories = {
                "../shaders/"
            },
            .apcDirectories = {
                "../shaders/"
            },
            .eFlags = PL_SHADER_FLAGS_AUTO_OUTPUT
        };
        gptShader->initialize(&tDefaultShaderOptions);
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
    {
        plGraphicsInit tGraphicsDesc = {
            #ifdef PL_CONFIG_DEBUG
            .eFlags = PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED | PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED
            #else
            .eFlags = PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED
            #endif
        };
        gptGfx->initialize(&tGraphicsDesc);
    }

    gptStarterCtx->ptWindow = tInit.ptWindow;

    // possibly create surface
    plSurface* ptSurface = gptGfx->create_surface(gptStarterCtx->ptWindow);
    gptStarterCtx->ptSurface = ptSurface;

    // possibly create device
    plDevice* ptDevice = pl_starter_create_device(ptSurface);
    gptStarterCtx->ptDevice = ptDevice;

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_TOOLS_EXT)
        gptTools->initialize((plToolsInit){.ptDevice = ptDevice});

    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptStarterCtx->atCmdPools[i] = gptGfx->create_command_pool(ptDevice, NULL);

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptStarterCtx->aptSemaphores[i] = gptGfx->create_semaphore(ptDevice, true);

    // create swapchain
    const plSwapchainInit tSwapInit = {
        .bVSync = !(gptStarterCtx->eFlags & PL_STARTER_FLAGS_VSYNC_OFF),
        .eSampleCount= (gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA) ? gptGfx->get_device_info(gptStarterCtx->ptDevice)->eMaxSampleCount : 1
    };
    plSwapchain* ptSwapchain = gptGfx->create_swapchain(ptDevice, gptStarterCtx->ptSurface, &tSwapInit);
    gptStarterCtx->ptSwapchain = ptSwapchain;

    gptStarterCtx->ptLocalBuddyAllocator = gptGpuAllocators->get_local_buddy_allocator(ptDevice);
    gptStarterCtx->ptLocalDedicatedAllocator = gptGpuAllocators->get_local_dedicated_allocator(ptDevice);

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {gptIOI->get_io()->tMainViewportSize.x + 400.0f, gptIOI->get_io()->tMainViewportSize.y + 400.0f, 1},
            .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .eType         = PL_TEXTURE_TYPE_2D,
            .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .pcDebugName   = "main depth texture",
            .eSampleCount  = tSwapInit.eSampleCount
        };

        plTexture* ptDepthTexture = NULL;
        gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

        plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
        if(ptDepthTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
            ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

        const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            ptDepthTexture->tMemoryRequirements.ulAlignment,
            "depth texture memory");

        gptGfx->bind_texture_to_memory(ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
    {
        plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain);
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
            .eFormat       = tInfo.eFormat,
            .uLayers       = 1,
            .uMips         = 1,
            .eType         = PL_TEXTURE_TYPE_2D,
            .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .pcDebugName   = "MSAA texture",
            .eSampleCount  = tSwapInit.eSampleCount
        };

        plTexture* ptResolveTexture = NULL;
        gptStarterCtx->tResolveTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptResolveTexture);


        plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
        if(ptResolveTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
            ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

        const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptResolveTexture->tMemoryRequirements.uMemoryTypeBits, 
            ptResolveTexture->tMemoryRequirements.ulSize,
            ptResolveTexture->tMemoryRequirements.ulAlignment,
            "msaa texture memory");

        gptGfx->bind_texture_to_memory(ptDevice, gptStarterCtx->tResolveTexture, &tDepthAllocation);
    }
    
    // if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER && gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
    //     pl__starter_create_render_pass_with_msaa_and_depth();
    // else if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    //     pl__starter_create_render_pass_with_depth();
    // else if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
    //     pl__starter_create_render_pass_with_msaa();
    // else
    //     pl__starter_create_render_pass();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // initialize
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DRAW_EXT)
    {
        plDrawInit tDrawInit = {
            .ptDevice = ptDevice
        };
        gptDraw->initialize(&tDrawInit);

        // create font atlas
        plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
        gptDraw->set_font_atlas(ptAtlas);
        gptStarterCtx->ptDefaultFont = gptDraw->add_default_font(ptAtlas);
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_starter_finalize(void)
{

    plFontAtlas* ptCurrentAtlas = gptDraw->get_current_font_atlas();
    if(gptStarterCtx->ptDefaultFont == NULL)
        gptStarterCtx->ptDefaultFont = gptDraw->get_first_font(ptCurrentAtlas);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~message extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_SCREEN_LOG_EXT)
        gptScreenLog->initialize((plScreenLogSettings){.ptFont = gptStarterCtx->ptDefaultFont});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ui extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_UI_EXT)
    {
        gptUI->initialize();
        gptUI->set_default_font(gptStarterCtx->ptDefaultFont);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~draw extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DRAW_EXT)
    {
        // build font atlas
        plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[0], "starter font atlas");
        gptDraw->build_font_atlas(ptCmdBuffer, gptDraw->get_current_font_atlas());
        gptGfx->wait_on_command_buffer(ptCmdBuffer);
        gptGfx->return_command_buffer(ptCmdBuffer);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~our own draw stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // register our app drawlist
    gptStarterCtx->ptFGDrawlist = gptDraw->request_2d_drawlist();
    gptStarterCtx->ptBGDrawlist = gptDraw->request_2d_drawlist();

    // request layers (allows drawing out of order)
    gptStarterCtx->ptFGLayer = gptDraw->request_2d_layer(gptStarterCtx->ptFGDrawlist);
    gptStarterCtx->ptBGLayer = gptDraw->request_2d_layer(gptStarterCtx->ptBGDrawlist);
}

void
pl_starter_resize(void)
{
    // perform any operations required during a window resize
    plIO* ptIO = gptIOI->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = !(gptStarterCtx->eFlags & PL_STARTER_FLAGS_VSYNC_OFF),
        .uWidth  = (uint32_t)(ptIO->tMainViewportSize.x * ptIO->tMainFramebufferScale.x),
        .uHeight = (uint32_t)(ptIO->tMainViewportSize.y * ptIO->tMainFramebufferScale.y),
        .eSampleCount = (gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA) ? gptGfx->get_device_info(gptStarterCtx->ptDevice)->eMaxSampleCount : 1
    };
    gptGfx->recreate_swapchain(gptStarterCtx->ptSwapchain, &tDesc);

    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain);

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        plVec3 tNewDimensions = {
                (float)tInfo.uWidth * ptIO->tMainFramebufferScale.x,
                (float)tInfo.uHeight * ptIO->tMainFramebufferScale.y,
                1};

        plTexture* ptTexture = gptGfx->get_texture(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);

        if(ptTexture->tDesc.tDimensions.x < tNewDimensions.x || ptTexture->tDesc.tDimensions.y < tNewDimensions.y || ptTexture->tDesc.eSampleCount != tInfo.eSampleCount)
        {
            gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);

            tNewDimensions.x += 400.0f;
            tNewDimensions.y += 400.0f;
            const plTextureDesc tDepthTextureDesc = {
                .tDimensions   = tNewDimensions,
                .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
                .uLayers       = 1,
                .uMips         = 1,
                .eType         = PL_TEXTURE_TYPE_2D,
                .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .pcDebugName   = "offscreen depth texture",
                .eSampleCount  = tInfo.eSampleCount
            };

            plTexture* ptDepthTexture = NULL;
            gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

            plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
            if(ptDepthTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
                ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

            const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
                ptDepthTexture->tMemoryRequirements.uMemoryTypeBits, 
                ptDepthTexture->tMemoryRequirements.ulSize,
                ptDepthTexture->tMemoryRequirements.ulAlignment,
                "depth texture memory");

            gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
        }
    }
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
    {

        plVec3 tNewDimensions = {
                (float)tInfo.uWidth,
                (float)tInfo.uHeight,
                1};

        plTexture* ptTexture = gptGfx->get_texture(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture);

        // if(ptTexture->tDesc.tDimensions.x < tNewDimensions.x || ptTexture->tDesc.tDimensions.y < tNewDimensions.y || ptTexture->tDesc.eSampleCount != tInfo.eSampleCount)
        {
            // tNewDimensions.x += 400.0f;
            // tNewDimensions.y += 400.0f;
            gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture);

            const plTextureDesc tDepthTextureDesc = {
                .tDimensions   = tNewDimensions,
                .eFormat       = tInfo.eFormat,
                .uLayers       = 1,
                .uMips         = 1,
                .eType         = PL_TEXTURE_TYPE_2D,
                .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                .pcDebugName   = "MSAA texture",
                .eSampleCount  = tInfo.eSampleCount
            };

            plTexture* ptResolveTexture = NULL;
            gptStarterCtx->tResolveTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptResolveTexture);

            plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
            if(ptResolveTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
                ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

            const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
                ptResolveTexture->tMemoryRequirements.uMemoryTypeBits, 
                ptResolveTexture->tMemoryRequirements.ulSize,
                ptResolveTexture->tMemoryRequirements.ulAlignment,
                "msaa texture memory");

            gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture, &tDepthAllocation);
        }

    }
}

void
pl_starter_cleanup(void)
{
    // ensure GPU is finished before cleanup
    gptGfx->flush_device(gptStarterCtx->ptDevice);
    gptGpuAllocators->cleanup(gptStarterCtx->ptDevice);

    // if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    //     gptGfx->destroy_texture(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
    // if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
    //     gptGfx->destroy_texture(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_command_pool(gptStarterCtx->atCmdPools[i]);
        gptGfx->cleanup_semaphore(gptStarterCtx->aptSemaphores[i]);
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_SHADER_EXT)
        gptShader->cleanup();
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DRAW_EXT)
    {
        gptDraw->cleanup_font_atlas(gptDraw->get_current_font_atlas());
        gptDraw->cleanup();
    }
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_UI_EXT)
        gptUI->cleanup();
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_CONSOLE_EXT)
        gptConsole->cleanup();
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_SCREEN_LOG_EXT)
        gptScreenLog->cleanup();

    gptGfx->cleanup_swapchain(gptStarterCtx->ptSwapchain);
    gptGfx->cleanup_surface(gptStarterCtx->ptSurface);
    gptGfx->cleanup_device(gptStarterCtx->ptDevice);

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
        gptGfx->cleanup();
}

bool
pl_starter_begin_frame(void)
{
    gptStarterCtx->bMainPassExplicit = false;

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
        gptProfile->begin_frame();

    plIO* ptIO = gptIOI->get_io();

    gptIOI->new_frame();

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DRAW_EXT)
        gptDraw->new_frame();

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_UI_EXT)
        gptUI->new_frame();

    // update statistics
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_STATS_EXT)
    {
        gptStats->new_frame();
        static double* pdFrameTimeCounter = NULL;
        static double* pdMemoryCounter = NULL;
        if(!pdFrameTimeCounter)
            pdFrameTimeCounter = gptStats->get_counter("frametime (ms)");
        if(!pdMemoryCounter)
            pdMemoryCounter = gptStats->get_counter("CPU memory");
        *pdFrameTimeCounter = (double)ptIO->fDeltaTime * 1000.0;
        *pdMemoryCounter = (double)gptMemory->get_memory_usage();
    }

    // begin new frame
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
        gptGfx->begin_frame(gptStarterCtx->ptDevice);
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarterCtx->atCmdPools[uCurrentFrameIndex];
    gptGfx->reset_command_pool(ptCmdPool, 0);

    if(gptStarterCtx->bVSyncChangeRequested)
    {
        pl_starter_resize();
        gptStarterCtx->bVSyncChangeRequested = false;
        if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false; 
    }

    if(gptStarterCtx->bMSAAActivateRequested)
    {
        pl__starter_activate_msaa();
        gptStarterCtx->bMSAAActivateRequested = false;
        if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }
    else if(gptStarterCtx->bMSAADeactivateRequested)
    {
        pl__starter_deactivate_msaa();
        gptStarterCtx->bMSAADeactivateRequested = false;
        if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }
    else if(gptStarterCtx->bDepthBufferActivateRequested)
    {
        pl__starter_activate_depth_buffer();
        gptStarterCtx->bDepthBufferActivateRequested = false;
        if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }
    else if(gptStarterCtx->bDepthBufferDeactivateRequested)
    {
        pl__starter_deactivate_depth_buffer();
        gptStarterCtx->bDepthBufferDeactivateRequested = false;
        if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
    {
        if(!gptGfx->acquire_swapchain_image(gptStarterCtx->ptSwapchain))
        {
            pl_starter_resize();
            if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
                gptProfile->end_frame();
            return false;
        }
    }

    return true;
}

plCommandBuffer*
pl_starter_begin_main_pass(void)
{
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarterCtx->atCmdPools[uCurrentFrameIndex];
    plCommandBuffer* ptCurrentCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "starter main pass");

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~
        gptGfx->begin_command_recording(ptCurrentCommandBuffer);

        uint32_t uImageCount = 0;
        plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptStarterCtx->ptSwapchain, &uImageCount);

        plRenderInfo tRenderInfo = {
            .tRenderArea = {
                .tMin = {0},
                .tMax = {.x = gptIOI->get_io()->tMainViewportSize.x, .y = gptIOI->get_io()->tMainViewportSize.y}
            },
            .atColorAttachments = {
                {
                    .tTexture       = (gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA) ? gptStarterCtx->tResolveTexture : atSwapchainImages[gptGfx->get_current_swapchain_image_index(gptStarterCtx->ptSwapchain)],
                    .eLoadOp        = PL_LOAD_OP_CLEAR,
                    .eStoreOp       = PL_STORE_OP_STORE,
                    .eUsage         = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
                    .tClearColor    = {0.0f, 0.0f, 0.0f, 1.0f},
                    .tResolveTexture = (gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA) ? atSwapchainImages[gptGfx->get_current_swapchain_image_index(gptStarterCtx->ptSwapchain)] : (plTextureHandle){0}
                }
            },
            .tDepthAttachment = {
                .tTexture        = gptStarterCtx->tDepthTexture,
                .eLoadOp         = PL_LOAD_OP_CLEAR,
                .eStoreOp        = PL_STORE_OP_DONT_CARE,
                .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .fClearZ         = gptStarterCtx->eFlags & PL_STARTER_FLAGS_REVERSE_Z ? 0.0f : 1.0f
            },
            .tStencilAttachment = {
                .tTexture        = gptStarterCtx->tDepthTexture,
                .eLoadOp         = PL_LOAD_OP_CLEAR,
                .eStoreOp        = PL_STORE_OP_DONT_CARE,
                .eUsage          = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .uClearStencil   = 0
            }
        };

        // begin main renderpass (directly to swapchain)
        gptGfx->begin_render_pass(ptCurrentCommandBuffer, &tRenderInfo, NULL);
        gptStarterCtx->ptCurrentCommandBuffer = ptCurrentCommandBuffer;

        gptStarterCtx->bMainPassExplicit = true;
    }
    else
    {
        gptStarterCtx->ptCurrentCommandBuffer = NULL;
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_CONSOLE_EXT)
    {
        if(gptIOI->is_key_pressed(PL_KEY_F1, false))
            gptConsole->open();
        gptConsole->update();
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_TOOLS_EXT)
        gptTools->update();

    return gptStarterCtx->ptCurrentCommandBuffer;
}

void
pl_starter_get_render_attachment_info(plRenderAttachmentInfo* ptInfoOut)
{
    plRenderAttachmentInfo tRenderAttachmentInfo = {
        .aeColorFormats = {
            gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).eFormat
        }
    };

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        tRenderAttachmentInfo.eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT;
        tRenderAttachmentInfo.eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT;
    }

    *ptInfoOut = tRenderAttachmentInfo;
}

void
pl_starter_end_main_pass(void)
{
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();

    

    // submits UI drawlist/layers
    plIO* ptIO = gptIOI->get_io();
    float fWidth = ptIO->tMainViewportSize.x;
    float fHeight = ptIO->tMainViewportSize.y;

    gptDraw->submit_2d_layer(gptStarterCtx->ptBGLayer);
    gptDraw->submit_2d_layer(gptStarterCtx->ptFGLayer);

    plRenderAttachmentInfo tRenderAttachmentInfo = {
        .aeColorFormats = {
            gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).eFormat
        }
    };

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        tRenderAttachmentInfo.eDepthFormat = PL_FORMAT_D32_FLOAT_S8_UINT;
        tRenderAttachmentInfo.eStencilFormat = PL_FORMAT_D32_FLOAT_S8_UINT;
    }
        
    gptDraw->submit_2d_drawlist(gptStarterCtx->ptBGDrawlist,
        gptStarterCtx->ptCurrentCommandBuffer,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).eSampleCount,
        &tRenderAttachmentInfo);

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_UI_EXT)
    {
        gptUI->end_frame();
        
        gptDraw->submit_2d_drawlist(gptUI->get_draw_list(), gptStarterCtx->ptCurrentCommandBuffer,
            ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y,
            gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).eSampleCount,
            &tRenderAttachmentInfo);
        gptDraw->submit_2d_drawlist(gptUI->get_debug_draw_list(), gptStarterCtx->ptCurrentCommandBuffer,
        ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).eSampleCount,
            &tRenderAttachmentInfo);
    }

    gptDraw->submit_2d_drawlist(gptStarterCtx->ptFGDrawlist, gptStarterCtx->ptCurrentCommandBuffer,
        ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).eSampleCount,
        &tRenderAttachmentInfo);

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_SCREEN_LOG_EXT)
    {
        plDrawList2D* ptMessageDrawlist = gptScreenLog->get_drawlist(fWidth - fWidth * 0.2f, 0.0f, fWidth * 0.2f, fHeight);
        gptDraw->submit_2d_drawlist(ptMessageDrawlist, gptStarterCtx->ptCurrentCommandBuffer,
            fWidth, fHeight, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).eSampleCount, &tRenderAttachmentInfo);
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
    {
        gptGfx->end_render_pass(gptStarterCtx->ptCurrentCommandBuffer);
        gptGfx->end_command_recording(gptStarterCtx->ptCurrentCommandBuffer);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

        const plSubmitInfo tSubmitInfo = {
            .uWaitSemaphoreCount     = 1,
            .atWaitSempahores        = {gptStarterCtx->aptSemaphores[uCurrentFrameIndex]},
            .auWaitSemaphoreValues   = {gptStarterCtx->aulNextTimelineValue[uCurrentFrameIndex]++},
            .uSignalSemaphoreCount   = 1,
            .atSignalSempahores      = {gptStarterCtx->aptSemaphores[uCurrentFrameIndex]},
            .auSignalSemaphoreValues = {gptStarterCtx->aulNextTimelineValue[uCurrentFrameIndex]},
        };

        if(!gptGfx->present(gptStarterCtx->ptCurrentCommandBuffer, &tSubmitInfo, &gptStarterCtx->ptSwapchain, 1))
        {
            pl_starter_resize();
        }

        gptGfx->return_command_buffer(gptStarterCtx->ptCurrentCommandBuffer);
        gptStarterCtx->ptCurrentCommandBuffer = NULL;
    }
}

void
pl_starter_end_frame(void)
{

    if(!gptStarterCtx->bMainPassExplicit)
    {
        pl_starter_begin_main_pass();
        pl_starter_end_main_pass();
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_PROFILE_EXT)
        gptProfile->end_frame();
}

plCommandBuffer*
pl_starter_get_command_buffer(void)
{
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarterCtx->atCmdPools[uCurrentFrameIndex];

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool, "starter");

    //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~

    gptGfx->begin_command_recording(ptCommandBuffer);

    return ptCommandBuffer;
}

void
pl_starter_submit_command_buffer(plCommandBuffer* ptCommandBuffer)
{
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();

    // end recording
    gptGfx->end_command_recording(ptCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uWaitSemaphoreCount     = 1,
        .atWaitSempahores        = {gptStarterCtx->aptSemaphores[uCurrentFrameIndex]},
        .auWaitSemaphoreValues   = {gptStarterCtx->aulNextTimelineValue[uCurrentFrameIndex]++},
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarterCtx->aptSemaphores[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {gptStarterCtx->aulNextTimelineValue[uCurrentFrameIndex]},
    };

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, &tSubmitInfo);
    gptGfx->return_command_buffer(ptCommandBuffer);
}

plDrawLayer2D*
pl_starter_get_foreground_layer(void)
{
    return gptStarterCtx->ptFGLayer;
}

plDrawLayer2D*
pl_starter_get_background_layer(void)
{
    return gptStarterCtx->ptBGLayer;
}

plDevice*
pl_starter_get_device(void)
{
    return gptStarterCtx->ptDevice;
}

plSwapchain*
pl_starter_get_swapchain(void)
{
    return gptStarterCtx->ptSwapchain;
}

void
pl_starter_set_swapchain(plSwapchain* ptSwapchain)
{
    gptStarterCtx->ptSwapchain = ptSwapchain;
}

plSurface*
pl_starter_get_surface(void)
{
    return gptStarterCtx->ptSurface;
}

plCommandPool*
pl_starter_get_current_command_pool(void)
{
    return gptStarterCtx->atCmdPools[gptGfx->get_current_frame_index()];
}

plTimelineSemaphore*
pl_starter_get_current_timeline_semaphore(void)
{
    return gptStarterCtx->aptSemaphores[gptGfx->get_current_frame_index()];
}

uint64_t            
pl_starter_get_current_timeline_value(void)
{
    return gptStarterCtx->aulNextTimelineValue[gptGfx->get_current_frame_index()];
}

plTimelineSemaphore*
pl_starter_get_last_timeline_semaphore(void)
{
    uint32_t uLastIndex = gptGfx->get_current_frame_index();
    uLastIndex--;
    uLastIndex = uLastIndex % gptGfx->get_frames_in_flight();
    return gptStarterCtx->aptSemaphores[uLastIndex];
}

uint64_t            
pl_starter_get_last_timeline_value(void)
{
    uint32_t uLastIndex = gptGfx->get_current_frame_index();
    uLastIndex--;
    uLastIndex = uLastIndex % gptGfx->get_frames_in_flight();
    return gptStarterCtx->aulNextTimelineValue[uLastIndex];
}

uint64_t            
pl_starter_increment_current_timeline_value(void)
{
    return ++(gptStarterCtx->aulNextTimelineValue[gptGfx->get_current_frame_index()]);
}

plFont*
pl_starter_get_default_font(void)
{
    return gptStarterCtx->ptDefaultFont;
}

void
pl_starter_set_default_font(plFont* ptFont)
{
    gptStarterCtx->ptDefaultFont = ptFont;
}

plCommandBuffer*
pl_starter_get_temporary_command_buffer(void)
{
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[gptGfx->get_current_frame_index()], "starter temp");
    gptGfx->begin_command_recording(ptCommandBuffer);
    return ptCommandBuffer;
}

void
pl_starter_submit_temporary_command_buffer(plCommandBuffer* ptCommandBuffer)
{
    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
}

plCommandBuffer*
pl_starter_get_raw_command_buffer(void)
{
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[gptGfx->get_current_frame_index()], "starter raw");
    return ptCommandBuffer;
}

void
pl_starter_return_raw_command_buffer(plCommandBuffer* ptCommandBuffer)
{
    gptGfx->return_command_buffer(ptCommandBuffer);
}

plDevice*
pl_starter_create_device(plSurface* ptSurface)
{

    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    gptGfx->enumerate_devices(atDeviceInfos, &uDeviceCount);

    // we will prefer discrete, then integrated GPUs
    int iBestDvcIdx = 0;
    int iDiscreteGPUIdx   = -1;
    int iIntegratedGPUIdx = -1;
    for(uint32_t i = 0; i < uDeviceCount; i++)
    {
        
        if(atDeviceInfos[i].eType == PL_DEVICE_TYPE_DISCRETE && iDiscreteGPUIdx == -1)
            iDiscreteGPUIdx = i;
        else if(atDeviceInfos[i].eType == PL_DEVICE_TYPE_INTEGRATED && iIntegratedGPUIdx == -1)
            iIntegratedGPUIdx = i;
    }

    if(iDiscreteGPUIdx > -1)
        iBestDvcIdx = iDiscreteGPUIdx;
    else if(iIntegratedGPUIdx > -1)
        iBestDvcIdx = iIntegratedGPUIdx;

    // create device
    const plDeviceInit tDeviceInit = {
        .uDeviceIdx               = iBestDvcIdx,
        .ptSurface                = ptSurface,
        .szDynamicBufferBlockSize = 134217728 / 2
    };
    plDevice* ptDevice = gptGfx->create_device(&tDeviceInit);
    gptDataRegistry->set_data("device", ptDevice); // used by debug extension
    return ptDevice;
}

void
pl_starter_activate_vsync(void)
{
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_VSYNC_OFF)
        gptStarterCtx->bVSyncChangeRequested = true;
    gptStarterCtx->eFlags &= ~PL_STARTER_FLAGS_VSYNC_OFF;
}

void
pl_starter_deactivate_vsync(void)
{
    if(!(gptStarterCtx->eFlags & PL_STARTER_FLAGS_VSYNC_OFF))
        gptStarterCtx->bVSyncChangeRequested = true;
    gptStarterCtx->eFlags |= PL_STARTER_FLAGS_VSYNC_OFF;
}

void
pl_starter_activate_msaa(void)
{
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
        return;
    gptStarterCtx->bMSAAActivateRequested = true;
}

void
pl_starter_deactivate_msaa(void)
{
    if(!(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA))
        return;
    gptStarterCtx->bMSAADeactivateRequested = true;
}

void
pl_starter_activate_depth_buffer(void)
{
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
        return;
    gptStarterCtx->bDepthBufferActivateRequested = true;
}

void
pl_starter_deactivate_depth_buffer(void)
{
    if(!(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER))
        return;
    gptStarterCtx->bDepthBufferDeactivateRequested = true;
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

static void
pl__starter_activate_msaa(void)
{
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
        return;
    
    gptStarterCtx->eFlags |= PL_STARTER_FLAGS_MSAA;

    plIO* ptIO = gptIOI->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = !(gptStarterCtx->eFlags & PL_STARTER_FLAGS_VSYNC_OFF),
        .uWidth  = (uint32_t)(ptIO->tMainViewportSize.x * ptIO->tMainFramebufferScale.x),
        .uHeight = (uint32_t)(ptIO->tMainViewportSize.y * ptIO->tMainFramebufferScale.y),
        .eSampleCount = gptGfx->get_device_info(gptStarterCtx->ptDevice)->eMaxSampleCount
    };
    gptGfx->recreate_swapchain(gptStarterCtx->ptSwapchain, &tDesc);

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA)
    {
        plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain);
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
            .eFormat       = tInfo.eFormat,
            .uLayers       = 1,
            .uMips         = 1,
            .eType         = PL_TEXTURE_TYPE_2D,
            .eUsage        = PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .pcDebugName   = "MSAA texture",
            .eSampleCount  = gptGfx->get_device_info(gptStarterCtx->ptDevice)->eMaxSampleCount
        };

        plTexture* ptResolveTexture = NULL;
        gptStarterCtx->tResolveTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptResolveTexture);

        plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
        if(ptResolveTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
            ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

        const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptResolveTexture->tMemoryRequirements.uMemoryTypeBits, 
            ptResolveTexture->tMemoryRequirements.ulSize,
            ptResolveTexture->tMemoryRequirements.ulAlignment,
            "msaa texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture, &tDepthAllocation);
    }

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
        plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain);
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {
                (float)tInfo.uWidth + 400.0f,
                (float)tInfo.uHeight + 400.0f,
                1},
            .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .eType         = PL_TEXTURE_TYPE_2D,
            .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .pcDebugName   = "main depth texture",
            .eSampleCount  = gptGfx->get_device_info(gptStarterCtx->ptDevice)->eMaxSampleCount
        };

        plTexture* ptDepthTexture = NULL;
        gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

        plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
        if(ptDepthTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
            ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

        const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            ptDepthTexture->tMemoryRequirements.ulAlignment,
            "depth texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);

        // pl__starter_create_render_pass_with_msaa_and_depth();
    }
    // else
    //     pl__starter_create_render_pass_with_msaa();
}

static void
pl__starter_deactivate_msaa(void)
{
    if(!(gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA))
        return;

    gptStarterCtx->eFlags &= ~PL_STARTER_FLAGS_MSAA;

    plIO* ptIO = gptIOI->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = !(gptStarterCtx->eFlags & PL_STARTER_FLAGS_VSYNC_OFF),
        .uWidth  = (uint32_t)(ptIO->tMainViewportSize.x * ptIO->tMainFramebufferScale.x),
        .uHeight = (uint32_t)(ptIO->tMainViewportSize.y * ptIO->tMainFramebufferScale.y),
        .eSampleCount = 1
    };
    gptGfx->recreate_swapchain(gptStarterCtx->ptSwapchain, &tDesc);

    gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture);

    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {
                gptIOI->get_io()->tMainViewportSize.x * ptIO->tMainFramebufferScale.x + 400.0f,
                gptIOI->get_io()->tMainViewportSize.y * ptIO->tMainFramebufferScale.y + 400.0f,
                1},
            .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .eType         = PL_TEXTURE_TYPE_2D,
            .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .pcDebugName   = "main depth texture",
            .eSampleCount  = 1
        };

        plTexture* ptDepthTexture = NULL;
        gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

        plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
        if(ptDepthTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
            ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

        const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            ptDepthTexture->tMemoryRequirements.ulAlignment,
            "depth texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
    }
}

static void
pl__starter_activate_depth_buffer(void)
{
    if(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
        return;
    
    gptStarterCtx->eFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    gptGfx->flush_device(gptStarterCtx->ptDevice);

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {
            gptIOI->get_io()->tMainViewportSize.x * gptIOI->get_io()->tMainFramebufferScale.x + 400.0f,
            gptIOI->get_io()->tMainViewportSize.y * gptIOI->get_io()->tMainFramebufferScale.y + 400.0f,
            1},
        .eFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .eType         = PL_TEXTURE_TYPE_2D,
        .eUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "main depth texture",
        .eSampleCount  = (gptStarterCtx->eFlags & PL_STARTER_FLAGS_MSAA) ? gptGfx->get_device_info(gptStarterCtx->ptDevice)->eMaxSampleCount : 1
    };

    plTexture* ptDepthTexture = NULL;
    gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

    plDeviceMemoryAllocatorI* ptAllocator = gptStarterCtx->ptLocalBuddyAllocator;
    if(ptDepthTexture->tMemoryRequirements.ulSize * 2 > gptGpuAllocators->get_buddy_block_size())
        ptAllocator = gptStarterCtx->ptLocalDedicatedAllocator;

    const plDeviceMemoryAllocation tDepthAllocation = ptAllocator->allocate(ptAllocator->ptInst,
        ptDepthTexture->tMemoryRequirements.uMemoryTypeBits, 
        ptDepthTexture->tMemoryRequirements.ulSize,
        ptDepthTexture->tMemoryRequirements.ulAlignment,
        "depth texture memory");

    gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
}

static void
pl__starter_deactivate_depth_buffer(void)
{
    if(!(gptStarterCtx->eFlags & PL_STARTER_FLAGS_DEPTH_BUFFER))
        return;

    gptStarterCtx->eFlags &= ~PL_STARTER_FLAGS_DEPTH_BUFFER;

    gptGfx->flush_device(gptStarterCtx->ptDevice);

    gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_starter_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plStarterI tApi = {
        .initialize                           = pl_starter_initialize,
        .finalize                             = pl_starter_finalize,
        .cleanup                              = pl_starter_cleanup,
        .begin_frame                          = pl_starter_begin_frame,
        .end_frame                            = pl_starter_end_frame,
        .begin_main_pass                      = pl_starter_begin_main_pass,
        .end_main_pass                        = pl_starter_end_main_pass,
        .resize                               = pl_starter_resize,
        .create_device                        = pl_starter_create_device,
        .get_device                           = pl_starter_get_device,
        .get_swapchain                        = pl_starter_get_swapchain,
        .set_swapchain                        = pl_starter_set_swapchain,
        .get_surface                          = pl_starter_get_surface,
        .get_current_command_pool             = pl_starter_get_current_command_pool,
        .get_current_timeline_semaphore       = pl_starter_get_current_timeline_semaphore,
        .get_last_timeline_semaphore          = pl_starter_get_last_timeline_semaphore,
        .get_current_timeline_value           = pl_starter_get_current_timeline_value,
        .get_last_timeline_value              = pl_starter_get_last_timeline_value,
        .increment_current_timeline_value     = pl_starter_increment_current_timeline_value,
        .get_default_font                     = pl_starter_get_default_font,
        .set_default_font                     = pl_starter_set_default_font,
        .get_temporary_command_buffer         = pl_starter_get_temporary_command_buffer,
        .get_raw_command_buffer               = pl_starter_get_raw_command_buffer,
        .submit_temporary_command_buffer      = pl_starter_submit_temporary_command_buffer,
        .return_raw_command_buffer            = pl_starter_return_raw_command_buffer,
        .get_command_buffer                   = pl_starter_get_command_buffer,
        .submit_command_buffer                = pl_starter_submit_command_buffer,
        .get_background_layer                 = pl_starter_get_background_layer,
        .get_foreground_layer                 = pl_starter_get_foreground_layer,
        .activate_msaa                        = pl_starter_activate_msaa,
        .deactivate_msaa                      = pl_starter_deactivate_msaa,
        .activate_depth_buffer                = pl_starter_activate_depth_buffer,
        .deactivate_depth_buffer              = pl_starter_deactivate_depth_buffer,
        .activate_vsync                       = pl_starter_activate_vsync,
        .deactivate_vsync                     = pl_starter_deactivate_vsync,
        .get_render_attachment_info           = pl_starter_get_render_attachment_info,
    };
    pl_set_api(ptApiRegistry, plStarterI, &tApi);

    gptDataRegistry  = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptIOI           = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptGpuAllocators = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);

    if(bReload)
    {
        gptStarterCtx = gptDataRegistry->get_data("plStarterContext");
    }
    else
    {
        static plStarterContext tStarterContext = {0};
        gptStarterCtx = &tStarterContext;
        gptDataRegistry->set_data("plStarterContext", gptStarterCtx);
    }

}

void
pl_unload_starter_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plStarterI* ptApi = pl_get_api_latest(ptApiRegistry, plStarterI);
    ptApiRegistry->remove_api(ptApi);
}
