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
#include "pl_draw_backend_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_shader_ext.h"
#include "pl_tools_ext.h"

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
    static const plDrawBackendI*   gptDrawBackend   = NULL;
    static const plIOI*            gptIOI           = NULL;
    static const plShaderI*        gptShader        = NULL;
    static const plProfileI*       gptProfile       = NULL;
    static const plConsoleI*       gptConsole       = NULL;
    static const plToolsI*         gptTools         = NULL;
    
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plStarterContext plStarterContext;

void                     pl_starter_initialize(plStarterInit);
void                     pl_starter_finalize(void);
void                     pl_starter_resize(void);
void                     pl_starter_cleanup(void);
bool                     pl_starter_begin_frame(void);
void                     pl_starter_end_frame(void);
plRenderEncoder*         pl_starter_begin_main_pass(void);
void                     pl_starter_end_main_pass(void);
plCommandBuffer*         pl_starter_get_command_buffer(void);
void                     pl_starter_submit_command_buffer(plCommandBuffer*);
plDrawLayer2D*           pl_starter_get_foreground_layer(void);
plDrawLayer2D*           pl_starter_get_background_layer(void);
plRenderEncoder*         pl_starter_get_current_encoder(void);
plDevice*                pl_starter_get_device(void);
plSwapchain*             pl_starter_get_swapchain(void);
plSurface*               pl_starter_get_surface(void);
plRenderPassHandle       pl_starter_get_render_pass(void);
plRenderPassLayoutHandle pl_starter_get_render_pass_layout(void);
plFont*                  pl_starter_get_default_font(void);
plDevice*                pl_starter_create_device(plSurface*);
plCommandBuffer*         pl_starter_get_temporary_command_buffer(void);
void                     pl_starter_submit_temporary_command_buffer(plCommandBuffer*);
plCommandBuffer*         pl_starter_get_raw_command_buffer(void);
void                     pl_starter_return_raw_command_buffer(plCommandBuffer*);
plBlitEncoder*           pl_starter_get_blit_encoder(void);
void                     pl_starter_return_blit_encoder(plBlitEncoder*);

// helpers
static void pl__starter_create_render_pass(void);
static void pl__starter_create_render_pass_with_msaa(void);
static void pl__starter_create_render_pass_with_depth(void);
static void pl__starter_create_render_pass_with_msaa_and_depth(void);
static void pl__starter_activate_msaa(void);
static void pl__starter_deactivate_msaa(void);
static void pl__starter_activate_depth_buffer(void);
static void pl__starter_deactivate_depth_buffer(void);

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plStarterContext
{
    plStarterFlags           tFlags;
    plWindow*                ptWindow;
    plDevice*                ptDevice;
    plSwapchain*             ptSwapchain;
    plSurface*               ptSurface;
    plFont*                  ptDefaultFont;
    plTimelineSemaphore*     aptSemaphores[PL_MAX_FRAMES_IN_FLIGHT];
    uint64_t                 aulNextTimelineValue[PL_MAX_FRAMES_IN_FLIGHT];
    plCommandPool*           atCmdPools[PL_MAX_FRAMES_IN_FLIGHT];
    plRenderPassHandle       tRenderPass;
    plRenderPassLayoutHandle tRenderPassLayout;
    plTextureHandle          tDepthTexture;
    plTextureHandle          tResolveTexture;
    bool                     bMainPassExplicit;
    bool                     bMSAAActivateRequested;
    bool                     bMSAADeactivateRequested;
    bool                     bDepthBufferActivateRequested;
    bool                     bDepthBufferDeactivateRequested;

    // drawing
    plDrawList2D*  ptFGDrawlist;
    plDrawList2D*  ptBGDrawlist;
    plDrawLayer2D* ptFGLayer;
    plDrawLayer2D* ptBGLayer;

    // current frame
    plRenderEncoder* ptCurrentEncoder;

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
    gptStarterCtx->tFlags = tInit.tFlags;

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_CONSOLE_EXT)
        gptConsole->initialize((plConsoleSettings){.tFlags = PL_CONSOLE_FLAGS_POPUP});

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_SHADER_EXT)
    {
        static const plShaderOptions tDefaultShaderOptions = {
            .apcIncludeDirectories = {
                "../shaders/"
            },
            .apcDirectories = {
                "../shaders/"
            },
            .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT
        };
        gptShader->initialize(&tDefaultShaderOptions);
    }

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
    {
        plGraphicsInit tGraphicsDesc = {
            .tFlags = PL_GRAPHICS_INIT_FLAGS_SWAPCHAIN_ENABLED | PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED
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

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_TOOLS_EXT)
        gptTools->initialize((plToolsInit){.ptDevice = ptDevice});

    // create command pools
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptStarterCtx->atCmdPools[i] = gptGfx->create_command_pool(ptDevice, NULL);

    // create timeline semaphores to syncronize GPU work submission
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
        gptStarterCtx->aptSemaphores[i] = gptGfx->create_semaphore(ptDevice, false);

    // create swapchain
    const plSwapchainInit tSwapInit = {
        .bVSync = true,
        .tSampleCount = (gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA) ? gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount : 1
    };
    plSwapchain* ptSwapchain = gptGfx->create_swapchain(ptDevice, gptStarterCtx->ptSurface, &tSwapInit);
    gptStarterCtx->ptSwapchain = ptSwapchain;

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        plBlitEncoder* ptEncoder = pl_starter_get_blit_encoder();
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {gptIOI->get_io()->tMainViewportSize.x, gptIOI->get_io()->tMainViewportSize.y, 1},
            .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .pcDebugName   = "main depth texture",
            .tSampleCount  = tSwapInit.tSampleCount
        };

        plTexture* ptDepthTexture = NULL;
        gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_DEVICE_LOCAL,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            "depth texture memory");

        gptGfx->bind_texture_to_memory(ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
        gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tDepthTexture, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);

        pl_starter_return_blit_encoder(ptEncoder);
    }

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
    {
        plBlitEncoder* ptEncoder = pl_starter_get_blit_encoder();
        plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain);
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
            .tFormat       = tInfo.tFormat,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .pcDebugName   = "MSAA texture",
            .tSampleCount  = tSwapInit.tSampleCount
        };

        plTexture* ptResolveTexture = NULL;
        gptStarterCtx->tResolveTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptResolveTexture);

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(ptDevice, 
            ptResolveTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_DEVICE_LOCAL,
            ptResolveTexture->tMemoryRequirements.uMemoryTypeBits,
            "msaa texture memory");

        gptGfx->bind_texture_to_memory(ptDevice, gptStarterCtx->tResolveTexture, &tDepthAllocation);
        gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tResolveTexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

        pl_starter_return_blit_encoder(ptEncoder);
    }
    
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER && gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        pl__starter_create_render_pass_with_msaa_and_depth();
    else if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
        pl__starter_create_render_pass_with_depth();
    else if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        pl__starter_create_render_pass_with_msaa();
    else
        pl__starter_create_render_pass();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // initialize
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DRAW_EXT)
    {
        gptDraw->initialize(NULL);
        gptDrawBackend->initialize(ptDevice);

        // create font atlas
        plFontAtlas* ptAtlas = gptDraw->create_font_atlas();
        gptDraw->set_font_atlas(ptAtlas);
        gptStarterCtx->ptDefaultFont = gptDraw->add_default_font(ptAtlas);
    }
}

void
pl_starter_finalize(void)
{

    plFontAtlas* ptCurrentAtlas = gptDraw->get_current_font_atlas();
    gptStarterCtx->ptDefaultFont = gptDraw->get_first_font(ptCurrentAtlas);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~message extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_SCREEN_LOG_EXT)
        gptScreenLog->initialize((plScreenLogSettings){.ptFont = gptStarterCtx->ptDefaultFont});

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ui extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_UI_EXT)
    {
        gptUI->initialize();
        gptUI->set_default_font(gptStarterCtx->ptDefaultFont);
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~draw extension~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DRAW_EXT)
    {
        // build font atlas
        plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[0]);
        gptDrawBackend->build_font_atlas(ptCmdBuffer, gptDraw->get_current_font_atlas());
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
        .bVSync  = true,
        .uWidth  = (uint32_t)ptIO->tMainViewportSize.x,
        .uHeight = (uint32_t)ptIO->tMainViewportSize.y,
        .tSampleCount = (gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA) ? gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount : 1
    };
    gptGfx->recreate_swapchain(gptStarterCtx->ptSwapchain, &tDesc);

    plCommandBuffer* ptCommandBuffer = NULL;
    plBlitEncoder* ptEncoder = NULL;
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER || gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
    {
        ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[0]);
        gptGfx->begin_command_recording(ptCommandBuffer, NULL);

        // begin blit pass, copy buffer, end pass
        ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    }

    plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);

        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {gptIOI->get_io()->tMainViewportSize.x, gptIOI->get_io()->tMainViewportSize.y, 1},
            .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .pcDebugName   = "offscreen depth texture",
            .tSampleCount  = tInfo.tSampleCount
        };

        plTexture* ptDepthTexture = NULL;
        gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(gptStarterCtx->ptDevice, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_DEVICE_LOCAL,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            "depth texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);

        gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tDepthTexture, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);
    }
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
    {
        gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture);

        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
            .tFormat       = tInfo.tFormat,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .pcDebugName   = "MSAA texture",
            .tSampleCount  = tInfo.tSampleCount
        };

        plTexture* ptResolveTexture = NULL;
        gptStarterCtx->tResolveTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptResolveTexture);

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(gptStarterCtx->ptDevice, 
            ptResolveTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_DEVICE_LOCAL,
            ptResolveTexture->tMemoryRequirements.uMemoryTypeBits,
            "msaa texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture, &tDepthAllocation);
        gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tResolveTexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

    }

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptStarterCtx->ptSwapchain, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {

        if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER && gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        {
            atMainAttachmentSets[i].atViewAttachments[0] = gptStarterCtx->tDepthTexture;
            atMainAttachmentSets[i].atViewAttachments[1] = gptStarterCtx->tResolveTexture;
            atMainAttachmentSets[i].atViewAttachments[2] = atSwapchainImages[i];
        }
        else if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
        {
            atMainAttachmentSets[i].atViewAttachments[0] = gptStarterCtx->tDepthTexture;
            atMainAttachmentSets[i].atViewAttachments[1] = atSwapchainImages[i];
        }
        else if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        {
            atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
            atMainAttachmentSets[i].atViewAttachments[1] = gptStarterCtx->tResolveTexture;
            
        }
        else
        {
            atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
        }
    }

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER || gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
    {
        gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
        gptGfx->end_blit_pass(ptEncoder);

        // finish recording
        gptGfx->end_command_recording(ptCommandBuffer);

        // submit command buffer
        gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
        gptGfx->wait_on_command_buffer(ptCommandBuffer);
        gptGfx->return_command_buffer(ptCommandBuffer);
    }

    gptGfx->update_render_pass_attachments(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPass, gptIOI->get_io()->tMainViewportSize, atMainAttachmentSets);
}

void
pl_starter_cleanup(void)
{
    // ensure GPU is finished before cleanup
    gptGfx->flush_device(gptStarterCtx->ptDevice);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
        gptGfx->destroy_texture(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        gptGfx->destroy_texture(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture);

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->cleanup_command_pool(gptStarterCtx->atCmdPools[i]);
        gptGfx->cleanup_semaphore(gptStarterCtx->aptSemaphores[i]);
    }

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_SHADER_EXT)
        gptShader->cleanup();
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DRAW_EXT)
    {
        gptDrawBackend->cleanup_font_atlas(gptDraw->get_current_font_atlas());
        gptDrawBackend->cleanup();
    }
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_UI_EXT)
        gptUI->cleanup();
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_CONSOLE_EXT)
        gptConsole->cleanup();
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_SCREEN_LOG_EXT)
        gptScreenLog->cleanup();

    gptGfx->cleanup_swapchain(gptStarterCtx->ptSwapchain);
    gptGfx->cleanup_surface(gptStarterCtx->ptSurface);
    gptGfx->cleanup_device(gptStarterCtx->ptDevice);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_GRAPHICS_EXT)
        gptGfx->cleanup();
}

bool
pl_starter_begin_frame(void)
{
    gptStarterCtx->bMainPassExplicit = false;

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_PROFILE_EXT)
        gptProfile->begin_frame();

    plIO* ptIO = gptIOI->get_io();

    gptIOI->new_frame();

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DRAW_EXT)
        gptDrawBackend->new_frame();

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_UI_EXT)
        gptUI->new_frame();

    // update statistics
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_STATS_EXT)
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
    gptGfx->begin_frame(gptStarterCtx->ptDevice);
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarterCtx->atCmdPools[uCurrentFrameIndex];
    gptGfx->reset_command_pool(ptCmdPool, 0);

    if(gptStarterCtx->bMSAAActivateRequested)
    {
        pl__starter_activate_msaa();
        gptStarterCtx->bMSAAActivateRequested = false;
        if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }
    else if(gptStarterCtx->bMSAADeactivateRequested)
    {
        pl__starter_deactivate_msaa();
        gptStarterCtx->bMSAADeactivateRequested = false;
        if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }
    else if(gptStarterCtx->bDepthBufferActivateRequested)
    {
        pl__starter_activate_depth_buffer();
        gptStarterCtx->bDepthBufferActivateRequested = false;
        if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }
    else if(gptStarterCtx->bDepthBufferDeactivateRequested)
    {
        pl__starter_deactivate_depth_buffer();
        gptStarterCtx->bDepthBufferDeactivateRequested = false;
        if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }

    if(!gptGfx->acquire_swapchain_image(gptStarterCtx->ptSwapchain))
    {
        pl_starter_resize();
        if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_PROFILE_EXT)
            gptProfile->end_frame();
        return false;
    }

    return true;
}

plRenderEncoder*
pl_starter_begin_main_pass(void)
{
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarterCtx->atCmdPools[uCurrentFrameIndex];
    plCommandBuffer* ptCurrentCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);

    //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarterCtx->aptSemaphores[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {gptStarterCtx->aulNextTimelineValue[uCurrentFrameIndex]++},
    };
    gptGfx->begin_command_recording(ptCurrentCommandBuffer, &tBeginInfo);

    gptStarterCtx->ptCurrentEncoder = gptGfx->begin_render_pass(ptCurrentCommandBuffer, gptStarterCtx->tRenderPass, NULL);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_CONSOLE_EXT)
    {
        if(gptIOI->is_key_pressed(PL_KEY_F1, false))
            gptConsole->open();
        gptConsole->update();
    }

    gptStarterCtx->bMainPassExplicit = true;

    return gptStarterCtx->ptCurrentEncoder;
}

void
pl_starter_end_main_pass(void)
{
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();

    plCommandBuffer* ptCurrentCommandBuffer = gptGfx->get_encoder_command_buffer(gptStarterCtx->ptCurrentEncoder);

    // submits UI drawlist/layers
    plIO* ptIO = gptIOI->get_io();
    float fWidth = ptIO->tMainViewportSize.x;
    float fHeight = ptIO->tMainViewportSize.y;

    gptDraw->submit_2d_layer(gptStarterCtx->ptBGLayer);
    gptDraw->submit_2d_layer(gptStarterCtx->ptFGLayer);

    gptDrawBackend->submit_2d_drawlist(gptStarterCtx->ptBGDrawlist, gptStarterCtx->ptCurrentEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tSampleCount);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_TOOLS_EXT)
        gptTools->update();

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_UI_EXT)
    {
        gptUI->end_frame();
        
        gptDrawBackend->submit_2d_drawlist(gptUI->get_draw_list(), gptStarterCtx->ptCurrentEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tSampleCount);
        gptDrawBackend->submit_2d_drawlist(gptUI->get_debug_draw_list(), gptStarterCtx->ptCurrentEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tSampleCount);
    }

    gptDrawBackend->submit_2d_drawlist(gptStarterCtx->ptFGDrawlist, gptStarterCtx->ptCurrentEncoder, ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tSampleCount);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_SCREEN_LOG_EXT)
    {
        plDrawList2D* ptMessageDrawlist = gptScreenLog->get_drawlist(fWidth - fWidth * 0.2f, 0.0f, fWidth * 0.2f, fHeight);
        gptDrawBackend->submit_2d_drawlist(ptMessageDrawlist, gptStarterCtx->ptCurrentEncoder, fWidth, fHeight, gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tSampleCount);
    }

    gptGfx->end_render_pass(gptStarterCtx->ptCurrentEncoder);
    gptStarterCtx->ptCurrentEncoder = NULL;

    // end recording
    gptGfx->end_command_recording(ptCurrentCommandBuffer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~submit work to GPU & present~~~~~~~~~~~~~~~~~~~~~~~

    const plSubmitInfo tSubmitInfo = {
        .uSignalSemaphoreCount   = 1,
        .atSignalSempahores      = {gptStarterCtx->aptSemaphores[uCurrentFrameIndex]},
        .auSignalSemaphoreValues = {gptStarterCtx->aulNextTimelineValue[uCurrentFrameIndex]},
    };

    if(!gptGfx->present(ptCurrentCommandBuffer, &tSubmitInfo, &gptStarterCtx->ptSwapchain, 1))
    {
        pl_starter_resize();
    }

    gptGfx->return_command_buffer(ptCurrentCommandBuffer);
}

void
pl_starter_end_frame(void)
{

    if(!gptStarterCtx->bMainPassExplicit)
    {
        pl_starter_begin_main_pass();
        pl_starter_end_main_pass();
    }

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_PROFILE_EXT)
        gptProfile->end_frame();
}

plCommandBuffer*
pl_starter_get_command_buffer(void)
{
    const uint32_t uCurrentFrameIndex = gptGfx->get_current_frame_index();
    plCommandPool* ptCmdPool = gptStarterCtx->atCmdPools[uCurrentFrameIndex];

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(ptCmdPool);

    //~~~~~~~~~~~~~~~~~~~~~~~~begin recording command buffer~~~~~~~~~~~~~~~~~~~~~~~

    const plBeginCommandInfo tBeginInfo = {
        .uWaitSemaphoreCount   = 1,
        .atWaitSempahores      = {gptStarterCtx->aptSemaphores[uCurrentFrameIndex]},
        .auWaitSemaphoreValues = {gptStarterCtx->aulNextTimelineValue[uCurrentFrameIndex]++},
    };
    gptGfx->begin_command_recording(ptCommandBuffer, &tBeginInfo);

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

plRenderEncoder*
pl_starter_get_current_encoder(void)
{
    return gptStarterCtx->ptCurrentEncoder;
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

plSurface*
pl_starter_get_surface(void)
{
    return gptStarterCtx->ptSurface;
}

plRenderPassHandle
pl_starter_get_render_pass(void)
{
    return gptStarterCtx->tRenderPass;
}

plRenderPassLayoutHandle
pl_starter_get_render_pass_layout(void)
{
    return gptStarterCtx->tRenderPassLayout;
}

plFont*
pl_starter_get_default_font(void)
{
    return gptStarterCtx->ptDefaultFont;
}

plCommandBuffer*
pl_starter_get_temporary_command_buffer(void)
{
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[gptGfx->get_current_frame_index()]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
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
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[gptGfx->get_current_frame_index()]);
    return ptCommandBuffer;
}

void
pl_starter_return_raw_command_buffer(plCommandBuffer* ptCommandBuffer)
{
    gptGfx->return_command_buffer(ptCommandBuffer);
}

plBlitEncoder*
pl_starter_get_blit_encoder(void)
{
    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[gptGfx->get_current_frame_index()]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    return ptEncoder;
}

void
pl_starter_return_blit_encoder(plBlitEncoder* ptEncoder)
{
    plCommandBuffer* ptCommandBuffer = gptGfx->get_blit_encoder_command_buffer(ptEncoder);

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
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
        
        if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_DISCRETE && iIntegratedGPUIdx == -1)
            iDiscreteGPUIdx = i;
        else if(atDeviceInfos[i].tType == PL_DEVICE_TYPE_INTEGRATED && iIntegratedGPUIdx == -1)
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
        .szDynamicBufferBlockSize = 134217728
    };
    plDevice* ptDevice = gptGfx->create_device(&tDeviceInit);
    gptDataRegistry->set_data("device", ptDevice); // used by debug extension
    return ptDevice;
}

void
pl_starter_activate_msaa(void)
{
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        return;
    gptStarterCtx->bMSAAActivateRequested = true;
}

void
pl_starter_deactivate_msaa(void)
{
    if(!(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA))
        return;
    gptStarterCtx->bMSAADeactivateRequested = true;
}

void
pl_starter_activate_depth_buffer(void)
{
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
        return;
    gptStarterCtx->bDepthBufferActivateRequested = true;
}

void
pl_starter_deactivate_depth_buffer(void)
{
    if(!(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER))
        return;
    gptStarterCtx->bDepthBufferDeactivateRequested = true;
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

static void
pl__starter_activate_msaa(void)
{
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        return;
    
    gptStarterCtx->tFlags |= PL_STARTER_FLAGS_MSAA;

    plIO* ptIO = gptIOI->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = true,
        .uWidth  = (uint32_t)ptIO->tMainViewportSize.x,
        .uHeight = (uint32_t)ptIO->tMainViewportSize.y,
        .tSampleCount = gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount
    };
    gptGfx->recreate_swapchain(gptStarterCtx->ptSwapchain, &tDesc);

    gptGfx->queue_render_pass_layout_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPassLayout);
    gptGfx->queue_render_pass_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPass);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
    {
        plBlitEncoder* ptEncoder = pl_starter_get_blit_encoder();
        plSwapchainInfo tInfo = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain);
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {(float)tInfo.uWidth, (float)tInfo.uHeight, 1},
            .tFormat       = tInfo.tFormat,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT,
            .pcDebugName   = "MSAA texture",
            .tSampleCount  = gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount
        };

        plTexture* ptResolveTexture = NULL;
        gptStarterCtx->tResolveTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptResolveTexture);

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(gptStarterCtx->ptDevice, 
            ptResolveTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_DEVICE_LOCAL,
            ptResolveTexture->tMemoryRequirements.uMemoryTypeBits,
            "msaa texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture, &tDepthAllocation);
        gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tResolveTexture, PL_TEXTURE_USAGE_COLOR_ATTACHMENT, 0);

        pl_starter_return_blit_encoder(ptEncoder);
    }

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
        plBlitEncoder* ptEncoder = pl_starter_get_blit_encoder();
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {gptIOI->get_io()->tMainViewportSize.x, gptIOI->get_io()->tMainViewportSize.y, 1},
            .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .pcDebugName   = "main depth texture",
            .tSampleCount  = gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount
        };

        plTexture* ptDepthTexture = NULL;
        gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(gptStarterCtx->ptDevice, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_DEVICE_LOCAL,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            "depth texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
        gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tDepthTexture, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);

        pl_starter_return_blit_encoder(ptEncoder);
        pl__starter_create_render_pass_with_msaa_and_depth();
    }
    else
        pl__starter_create_render_pass_with_msaa();
}

static void
pl__starter_deactivate_msaa(void)
{
    if(!(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA))
        return;

    gptStarterCtx->tFlags &= ~PL_STARTER_FLAGS_MSAA;

    plIO* ptIO = gptIOI->get_io();
    plSwapchainInit tDesc = {
        .bVSync  = true,
        .uWidth  = (uint32_t)ptIO->tMainViewportSize.x,
        .uHeight = (uint32_t)ptIO->tMainViewportSize.y,
        .tSampleCount = 1
    };
    gptGfx->recreate_swapchain(gptStarterCtx->ptSwapchain, &tDesc);

    gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tResolveTexture);
    gptGfx->queue_render_pass_layout_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPassLayout);
    gptGfx->queue_render_pass_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPass);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
    {
        gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
        plBlitEncoder* ptEncoder = pl_starter_get_blit_encoder();
        const plTextureDesc tDepthTextureDesc = {
            .tDimensions   = {gptIOI->get_io()->tMainViewportSize.x, gptIOI->get_io()->tMainViewportSize.y, 1},
            .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
            .uLayers       = 1,
            .uMips         = 1,
            .tType         = PL_TEXTURE_TYPE_2D,
            .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
            .pcDebugName   = "main depth texture",
            .tSampleCount  = 1
        };

        plTexture* ptDepthTexture = NULL;
        gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

        const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(gptStarterCtx->ptDevice, 
            ptDepthTexture->tMemoryRequirements.ulSize,
            PL_MEMORY_FLAGS_DEVICE_LOCAL,
            ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
            "depth texture memory");

        gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
        gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tDepthTexture, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);

        pl_starter_return_blit_encoder(ptEncoder);
        pl__starter_create_render_pass_with_depth();
    }
    else
        pl__starter_create_render_pass();
}

static void
pl__starter_activate_depth_buffer(void)
{
    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER)
        return;
    
    gptStarterCtx->tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    gptGfx->flush_device(gptStarterCtx->ptDevice);

    gptGfx->queue_render_pass_layout_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPassLayout);
    gptGfx->queue_render_pass_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPass);

    plBlitEncoder* ptEncoder = pl_starter_get_blit_encoder();
    const plTextureDesc tDepthTextureDesc = {
        .tDimensions   = {gptIOI->get_io()->tMainViewportSize.x, gptIOI->get_io()->tMainViewportSize.y, 1},
        .tFormat       = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
        .pcDebugName   = "main depth texture",
        .tSampleCount  = (gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA) ? gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount : 1
    };

    plTexture* ptDepthTexture = NULL;
    gptStarterCtx->tDepthTexture = gptGfx->create_texture(gptStarterCtx->ptDevice, &tDepthTextureDesc, &ptDepthTexture);

    const plDeviceMemoryAllocation tDepthAllocation = gptGfx->allocate_memory(gptStarterCtx->ptDevice, 
        ptDepthTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptDepthTexture->tMemoryRequirements.uMemoryTypeBits,
        "depth texture memory");

    gptGfx->bind_texture_to_memory(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture, &tDepthAllocation);
    gptGfx->set_texture_usage(ptEncoder, gptStarterCtx->tDepthTexture, PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 0);

    pl_starter_return_blit_encoder(ptEncoder);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        pl__starter_create_render_pass_with_msaa_and_depth();
    else
        pl__starter_create_render_pass_with_depth();
}

static void
pl__starter_deactivate_depth_buffer(void)
{
    if(!(gptStarterCtx->tFlags & PL_STARTER_FLAGS_DEPTH_BUFFER))
        return;

    gptStarterCtx->tFlags &= ~PL_STARTER_FLAGS_DEPTH_BUFFER;

    gptGfx->flush_device(gptStarterCtx->ptDevice);

    gptGfx->queue_texture_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tDepthTexture);
    gptGfx->queue_render_pass_layout_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPassLayout);
    gptGfx->queue_render_pass_for_deletion(gptStarterCtx->ptDevice, gptStarterCtx->tRenderPass);

    if(gptStarterCtx->tFlags & PL_STARTER_FLAGS_MSAA)
        pl__starter_create_render_pass_with_msaa();
    else
        pl__starter_create_render_pass();
}

static void
pl__starter_create_render_pass(void)
{
    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tFormat },
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0}
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptStarterCtx->tRenderPassLayout = gptGfx->create_render_pass_layout(gptStarterCtx->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = gptStarterCtx->tRenderPassLayout,
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = gptIOI->get_io()->tMainViewportSize.x, .y = gptIOI->get_io()->tMainViewportSize.y},
        .ptSwapchain = gptStarterCtx->ptSwapchain
    };

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptStarterCtx->ptSwapchain, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
    }
    gptStarterCtx->tRenderPass = gptGfx->create_render_pass(gptStarterCtx->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);
}

static void
pl__starter_create_render_pass_with_msaa(void)
{
    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tFormat, .bResolve = true }, // swapchain
            { .tFormat = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tFormat, .tSamples = gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount}, // msaa
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptStarterCtx->tRenderPassLayout = gptGfx->create_render_pass_layout(gptStarterCtx->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = gptStarterCtx->tRenderPassLayout,
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
                .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = gptIOI->get_io()->tMainViewportSize.x, .y = gptIOI->get_io()->tMainViewportSize.y},
        .ptSwapchain = gptStarterCtx->ptSwapchain
    };

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptStarterCtx->ptSwapchain, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = atSwapchainImages[i];
        atMainAttachmentSets[i].atViewAttachments[1] = gptStarterCtx->tResolveTexture;
    }
    gptStarterCtx->tRenderPass = gptGfx->create_render_pass(gptStarterCtx->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);
}

static void
pl__starter_create_render_pass_with_depth(void)
{
    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true }, // depth buffer
            { .tFormat = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tFormat },
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 2,
                .auRenderTargets = {0, 1}
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptStarterCtx->tRenderPassLayout = gptGfx->create_render_pass_layout(gptStarterCtx->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = gptStarterCtx->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .fClearZ         = 1.0f
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = gptIOI->get_io()->tMainViewportSize.x, .y = gptIOI->get_io()->tMainViewportSize.y},
        .ptSwapchain = gptStarterCtx->ptSwapchain
    };

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptStarterCtx->ptSwapchain, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = gptStarterCtx->tDepthTexture;
        atMainAttachmentSets[i].atViewAttachments[1] = atSwapchainImages[i];
    }
    gptStarterCtx->tRenderPass = gptGfx->create_render_pass(gptStarterCtx->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
}

static void
pl__starter_create_render_pass_with_msaa_and_depth(void)
{
    const plRenderPassLayoutDesc tMainRenderPassLayoutDesc = {
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT, .bDepth = true, .tSamples = gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount }, // depth buffer
            { .tFormat = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tFormat, .tSamples = gptGfx->get_device_info(gptStarterCtx->ptDevice)->tMaxSampleCount}, // msaa
            { .tFormat = gptGfx->get_swapchain_info(gptStarterCtx->ptSwapchain).tFormat, .bResolve = true }, // swapchain

        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 3,
                .auRenderTargets = {0, 1, 2}
            }
        },
        .atSubpassDependencies = {
            {
                .uSourceSubpass = UINT32_MAX,
                .uDestinationSubpass = 0,
                .tSourceStageMask = PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tSourceAccessMask = PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
            {
                .uSourceSubpass = 0,
                .uDestinationSubpass = UINT32_MAX,
                .tSourceStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS,
                .tDestinationStageMask = PL_PIPELINE_STAGE_FRAGMENT_SHADER | PL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT | PL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS | PL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS | PL_PIPELINE_STAGE_COMPUTE_SHADER,
                .tSourceAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
                .tDestinationAccessMask = PL_ACCESS_SHADER_READ | PL_ACCESS_COLOR_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE | PL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ,
            },
        }
    };
    gptStarterCtx->tRenderPassLayout = gptGfx->create_render_pass_layout(gptStarterCtx->ptDevice, &tMainRenderPassLayoutDesc);

    // create main render pass
    plRenderPassDesc tMainRenderPassDesc = {
        .tLayout = gptStarterCtx->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tCurrentUsage   = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .tNextUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
                .fClearZ         = 1.0f
        },
        .tResolveTarget = { // swapchain image
            .tLoadOp       = PL_LOAD_OP_DONT_CARE,
            .tStoreOp      = PL_STORE_OP_STORE,
            .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
            .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
            .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
        },
        .atColorTargets = {
            {
                .tLoadOp       = PL_LOAD_OP_CLEAR,
                .tStoreOp      = PL_STORE_OP_STORE,
                .tCurrentUsage = PL_TEXTURE_USAGE_UNSPECIFIED,
                .tNextUsage    = PL_TEXTURE_USAGE_PRESENT,
                .tClearColor   = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions = {.x = gptIOI->get_io()->tMainViewportSize.x, .y = gptIOI->get_io()->tMainViewportSize.y},
        .ptSwapchain = gptStarterCtx->ptSwapchain
    };

    plCommandBuffer* ptCommandBuffer = gptGfx->request_command_buffer(gptStarterCtx->atCmdPools[0]);
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);

    // begin blit pass, copy buffer, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);
    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);

    uint32_t uImageCount = 0;
    plTextureHandle* atSwapchainImages = gptGfx->get_swapchain_images(gptStarterCtx->ptSwapchain, &uImageCount);
    plRenderPassAttachments atMainAttachmentSets[16] = {0};
    for(uint32_t i = 0; i < uImageCount; i++)
    {
        atMainAttachmentSets[i].atViewAttachments[0] = gptStarterCtx->tDepthTexture;
        atMainAttachmentSets[i].atViewAttachments[1] = gptStarterCtx->tResolveTexture;
        atMainAttachmentSets[i].atViewAttachments[2] = atSwapchainImages[i];
    }
    gptStarterCtx->tRenderPass = gptGfx->create_render_pass(gptStarterCtx->ptDevice, &tMainRenderPassDesc, atMainAttachmentSets);

    gptGfx->pipeline_barrier_blit(ptEncoder, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);
    gptGfx->return_command_buffer(ptCommandBuffer);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_starter_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plStarterI tApi = {
        .initialize                      = pl_starter_initialize,
        .finalize                        = pl_starter_finalize,
        .cleanup                         = pl_starter_cleanup,
        .begin_frame                     = pl_starter_begin_frame,
        .end_frame                       = pl_starter_end_frame,
        .begin_main_pass                 = pl_starter_begin_main_pass,
        .end_main_pass                   = pl_starter_end_main_pass,
        .resize                          = pl_starter_resize,
        .create_device                   = pl_starter_create_device,
        .get_device                      = pl_starter_get_device,
        .get_swapchain                   = pl_starter_get_swapchain,
        .get_surface                     = pl_starter_get_surface,
        .get_render_pass                 = pl_starter_get_render_pass,
        .get_render_pass_layout          = pl_starter_get_render_pass_layout,
        .get_default_font                = pl_starter_get_default_font,
        .get_temporary_command_buffer    = pl_starter_get_temporary_command_buffer,
        .get_raw_command_buffer          = pl_starter_get_raw_command_buffer,
        .submit_temporary_command_buffer = pl_starter_submit_temporary_command_buffer,
        .return_raw_command_buffer       = pl_starter_return_raw_command_buffer,
        .get_blit_encoder                = pl_starter_get_blit_encoder,
        .get_command_buffer              = pl_starter_get_command_buffer,
        .submit_command_buffer           = pl_starter_submit_command_buffer,
        .return_blit_encoder             = pl_starter_return_blit_encoder,
        .get_background_layer            = pl_starter_get_background_layer,
        .get_foreground_layer            = pl_starter_get_foreground_layer,
        .activate_msaa                   = pl_starter_activate_msaa,
        .deactivate_msaa                 = pl_starter_deactivate_msaa,
        .activate_depth_buffer           = pl_starter_activate_depth_buffer,
        .deactivate_depth_buffer         = pl_starter_deactivate_depth_buffer,
    };
    pl_set_api(ptApiRegistry, plStarterI, &tApi);

    gptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptMemory       = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptGfx          = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptScreenLog    = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptUI           = pl_get_api_latest(ptApiRegistry, plUiI);
    gptDraw         = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend  = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptIOI          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader       = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptStats        = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptConsole      = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptProfile      = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptShader       = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptTools        = pl_get_api_latest(ptApiRegistry, plToolsI);

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

PL_EXPORT void
pl_unload_starter_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plStarterI* ptApi = pl_get_api_latest(ptApiRegistry, plStarterI);
    ptApiRegistry->remove_api(ptApi);
}
