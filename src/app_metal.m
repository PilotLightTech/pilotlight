/*
   metal_app.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "pilotlight.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_memory.h"
#include "pl_draw_metal.h"
#include "pl_math.h"
#include "pl_registry.h" // data registry
#include "pl_ext.h"      // extension registry
#include "pl_ui.h"

// extensions
#include "pl_draw_extension.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMetalDevice
{
    id<MTLDevice> device;
} plMetalDevice;

typedef struct _plMetalGraphics
{
    id<MTLCommandQueue> cmdQueue;
    uint32_t currentFrame;
    CAMetalLayer *metalLayer;
} plMetalGraphics;

typedef struct plAppData_t
{
    plMetalDevice            device;
    plMetalGraphics          graphics;
    id<MTLTexture>           depthTarget;
    MTLRenderPassDescriptor* drawableRenderDescriptor;
    plDrawContext            ctx;
    plDrawList               drawlist;
    plDrawLayer*             fgDrawLayer;
    plDrawLayer*             bgDrawLayer;
    plFontAtlas              fontAtlas;
    plProfileContext         tProfileCtx;
    plLogContext             tLogCtx;
    plMemoryContext          tMemoryCtx;
    plDataRegistry           tDataRegistryCtx;
    plExtensionRegistry      tExtensionRegistryCtx;
    plUiContext              tUiContext;

    // extension apis
    plDrawExtension*         ptDrawExtApi;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plIOContext* ptIOCtx, plAppData* ptAppData)
{

    if(ptAppData) // reload
    {
        pl_set_log_context(&ptAppData->tLogCtx);
        pl_set_profile_context(&ptAppData->tProfileCtx);
        pl_set_memory_context(&ptAppData->tMemoryCtx);
        pl_set_data_registry(&ptAppData->tDataRegistryCtx);
        pl_set_extension_registry(&ptAppData->tExtensionRegistryCtx);
        pl_set_io_context(ptIOCtx);

        plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
        ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

        return ptAppData;
    }
    
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->device.device = ptIOCtx->pBackendPlatformData;

    pl_set_io_context(ptIOCtx);

    // setup memory context
    pl_initialize_memory_context(&ptAppData->tMemoryCtx);

    // setup profiling context
    pl_initialize_profile_context(&ptAppData->tProfileCtx);

    // setup data registry
    pl_initialize_data_registry(&ptAppData->tDataRegistryCtx);

    // setup logging
    pl_initialize_log_context(&ptAppData->tLogCtx);
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(0, "Setup logging");

    // setup extension registry
    pl_initialize_extension_registry(&ptAppData->tExtensionRegistryCtx);
    pl_register_data("memory", &ptAppData->tMemoryCtx);
    pl_register_data("profile", &ptAppData->tProfileCtx);
    pl_register_data("log", &ptAppData->tLogCtx);
    pl_register_data("io", ptIOCtx);
    pl_register_data("draw", &ptAppData->ctx);

    plExtension tExtension = {0};
    pl_get_draw_extension_info(&tExtension);
    pl_load_extension(&tExtension);

    plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
    ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

    // create command queue
    ptAppData->device.device = ptIOCtx->pBackendPlatformData;
    ptAppData->graphics.cmdQueue = [ptAppData->device.device newCommandQueue];

    // render pass descriptor
    ptAppData->drawableRenderDescriptor = [MTLRenderPassDescriptor new];

    // color attachment
    ptAppData->drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    ptAppData->drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    ptAppData->drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

    // depth attachment
    ptAppData->drawableRenderDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    ptAppData->drawableRenderDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    ptAppData->drawableRenderDescriptor.depthAttachment.clearDepth = 1.0;

    // create draw context
    pl_initialize_draw_context_metal(&ptAppData->ctx, ptAppData->device.device);

    // create draw list & layers
    pl_register_drawlist(&ptAppData->ctx, &ptAppData->drawlist);
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(&ptAppData->ctx, &ptAppData->fontAtlas);

    // ui
    pl_ui_setup_context(&ptAppData->ctx, &ptAppData->tUiContext);
    ptAppData->tUiContext.ptFont = &ptAppData->fontAtlas.sbFonts[0];

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{

    // clean up contexts
    pl_cleanup_font_atlas(&ptAppData->fontAtlas);
    pl_cleanup_draw_context(&ptAppData->ctx);
    pl_ui_cleanup_context(&ptAppData->tUiContext);
    pl_cleanup_profile_context();
    pl_cleanup_extension_registry();
    pl_cleanup_log_context();
    pl_cleanup_data_registry();
    pl_cleanup_memory_context();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{    
    plIOContext* ptIOCtx = pl_get_io_context();

    // recreate depth texture
    MTLTextureDescriptor *depthTargetDescriptor = [MTLTextureDescriptor new];
    depthTargetDescriptor.width       = (uint32_t)ptIOCtx->afMainViewportSize[0];
    depthTargetDescriptor.height      = (uint32_t)ptIOCtx->afMainViewportSize[1];
    depthTargetDescriptor.pixelFormat = MTLPixelFormatDepth32Float;
    depthTargetDescriptor.storageMode = MTLStorageModePrivate;
    depthTargetDescriptor.usage       = MTLTextureUsageRenderTarget;
    ptAppData->depthTarget = [ptAppData->device.device newTextureWithDescriptor:depthTargetDescriptor];
    ptAppData->drawableRenderDescriptor.depthAttachment.texture = ptAppData->depthTarget;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    pl_handle_extension_reloads();

    pl_new_io_frame();

    plIOContext* ptIOCtx = pl_get_io_context();
    ptAppData->graphics.metalLayer = ptIOCtx->pBackendRendererData;

    ptAppData->graphics.currentFrame++;

    // begin profiling frame
    pl_begin_profile_frame(ptAppData->graphics.currentFrame);

    // request command buffer
    id<MTLCommandBuffer> commandBuffer = [ptAppData->graphics.cmdQueue commandBuffer];

    // get next drawable
    id<CAMetalDrawable> currentDrawable = [ptAppData->graphics.metalLayer nextDrawable];

    if(!currentDrawable)
        return;

    // set colorattachment to next drawable
    ptAppData->drawableRenderDescriptor.colorAttachments[0].texture = currentDrawable.texture;

    pl_new_draw_frame_metal(&ptAppData->ctx, ptAppData->drawableRenderDescriptor);
    pl_ui_new_frame(&ptAppData->tUiContext);

    // create render command encoder
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:ptAppData->drawableRenderDescriptor];

    ptAppData->ptDrawExtApi->pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "extension baby");

    // draw profiling info
    pl_begin_profile_sample("Draw Profiling Info");

    char cPProfileValue[64] = {0};
    for(uint32_t i = 0u; i < pl_sb_size(ptAppData->tProfileCtx.ptLastFrame->sbtSamples); i++)
    {
        plProfileSample* tPSample = &ptAppData->tProfileCtx.ptLastFrame->sbtSamples[i];
        pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, tPSample->pcName, 0.0f);
        plVec2 sampleTextSize = pl_calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, tPSample->pcName, 0.0f);
        pl_sprintf(cPProfileValue, ": %0.5f", tPSample->dDuration);
        pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){sampleTextSize.x + 15.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, cPProfileValue, 0.0f);
    }
    pl_end_profile_sample();

    // draw commands
    pl_begin_profile_sample("Add draw commands");
    pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){300.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 50.0f}, (plVec2){300.0f, 150.0f}, (plVec2){350.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl__begin_profile_sample("Calculate text size");
    plVec2 textSize = pl_calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl__end_profile_sample();
    pl_add_rect_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 10.0f}, (plVec2){300.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(ptAppData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    pl_end_profile_sample();

    static bool bOpen = true;


    if(pl_ui_begin_window("Pilot Light", NULL, false))
    {
        pl_ui_text("%.6f ms", ptIOCtx->fDeltaTime);

        pl_ui_checkbox("Camera Info", &bOpen);
        

        pl_ui_end_window();
    }

    if(bOpen)
    {
        if(pl_ui_begin_window("Camera Info", &bOpen, true))
        {
            pl_ui_text("Pos: %.3f, %.3f, %.3f", 0.0f, 0.0f, 0.0f); 
        }
        pl_ui_end_window();
    }

    if(pl_ui_begin_window("UI Demo", NULL, false))
    {
        pl_ui_progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
        if(pl_ui_button("Press me"))
            bOpen = true;
        static bool bOpen0 = false;
        if(pl_ui_tree_node("Root Node", &bOpen0))
        {
            static bool bOpen1 = false;
            if(pl_ui_tree_node("Child 1", &bOpen1))
            {
                if(pl_ui_button("Press me"))
                    bOpen = true;
                pl_ui_tree_pop();
            }
            static bool bOpen2 = false;
            if(pl_ui_tree_node("Child 2", &bOpen2))
            {
                pl_ui_button("Press me");
                pl_ui_tree_pop();
            }
            pl_ui_tree_pop();
        }
        static bool bOpen3 = false;
        if(pl_ui_collapsing_header("Collapsing Header", &bOpen3))
        {
            pl_ui_checkbox("Camera window2", &bOpen);
        }
    }
    pl_ui_end_window();

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    pl_submit_draw_layer(ptAppData->bgDrawLayer);
    pl_submit_draw_layer(ptAppData->fgDrawLayer);
    pl_end_profile_sample();

    pl_ui_render();

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    ptAppData->ctx.tFrameBufferScale.x = ptIOCtx->afMainFramebufferScale[0];
    ptAppData->ctx.tFrameBufferScale.y = ptIOCtx->afMainFramebufferScale[1];
    pl_submit_drawlist_metal(&ptAppData->drawlist, ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    pl_submit_drawlist_metal(ptAppData->tUiContext.ptDrawlist, ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    pl_end_profile_sample();

    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];

    pl_end_io_frame();
    pl_ui_end_frame();

    // end profiling frame
    pl_end_profile_frame();
}