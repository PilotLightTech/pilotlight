/*
   metal_app.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_setup
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_graphics_metal.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_memory.h"
#include "pl_draw_metal.h"
#include "pl_math.h"
#include "pl_registry.h" // data registry
#include "pl_ext.h"      // extension registry

// extensions
#include "pl_draw_extension.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

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
    
    plAppData* tPNewData = malloc(sizeof(plAppData));
    memset(tPNewData, 0, sizeof(plAppData));
    tPNewData->device.device = ptIOCtx->pBackendPlatformData;

    pl_set_io_context(ptIOCtx);

    // setup memory context
    pl_initialize_memory_context(&tPNewData->tMemoryCtx);

    // setup profiling context
    pl_initialize_profile_context(&tPNewData->tProfileCtx);

    // setup data registry
    pl_initialize_data_registry(&tPNewData->tDataRegistryCtx);

    // setup logging
    pl_initialize_log_context(&tPNewData->tLogCtx);
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(0, "Setup logging");

    // setup extension registry
    pl_initialize_extension_registry(&tPNewData->tExtensionRegistryCtx);
    pl_register_data("memory", &tPNewData->tMemoryCtx);
    pl_register_data("profile", &tPNewData->tProfileCtx);
    pl_register_data("log", &tPNewData->tLogCtx);
    pl_register_data("io", ptIOCtx);

    plExtension tExtension = {0};
    pl_get_draw_extension_info(&tExtension);
    pl_load_extension(&tExtension);

    plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
    tPNewData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

    return tPNewData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_setup(plAppData* appData)
{

    plIOContext* ptIOCtx = pl_get_io_context();

    // create command queue
    appData->device.device = ptIOCtx->pBackendPlatformData;
    appData->graphics.cmdQueue = [appData->device.device newCommandQueue];

    // render pass descriptor
    appData->drawableRenderDescriptor = [MTLRenderPassDescriptor new];

    // color attachment
    appData->drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    appData->drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    appData->drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.01, 0, 0, 1);

    // depth attachment
    appData->drawableRenderDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    appData->drawableRenderDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    appData->drawableRenderDescriptor.depthAttachment.clearDepth = 1.0;

    // create draw context
    pl_initialize_draw_context_metal(&appData->ctx, appData->device.device);

    // create draw list & layers
    pl_register_drawlist(&appData->ctx, &appData->drawlist);
    appData->bgDrawLayer = pl_request_draw_layer(&appData->drawlist, "Background Layer");
    appData->fgDrawLayer = pl_request_draw_layer(&appData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&appData->fontAtlas);
    pl_build_font_atlas(&appData->ctx, &appData->fontAtlas);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* appData)
{

    // clean up contexts
    pl_cleanup_font_atlas(&appData->fontAtlas);
    pl_cleanup_draw_context(&appData->ctx);
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
pl_app_resize(plAppData* appData)
{    
    plIOContext* ptIOCtx = pl_get_io_context();

    // recreate depth texture
    MTLTextureDescriptor *depthTargetDescriptor = [MTLTextureDescriptor new];
    depthTargetDescriptor.width       = (uint32_t)ptIOCtx->afMainViewportSize[0];
    depthTargetDescriptor.height      = (uint32_t)ptIOCtx->afMainViewportSize[1];
    depthTargetDescriptor.pixelFormat = MTLPixelFormatDepth32Float;
    depthTargetDescriptor.storageMode = MTLStorageModePrivate;
    depthTargetDescriptor.usage       = MTLTextureUsageRenderTarget;
    appData->depthTarget = [appData->device.device newTextureWithDescriptor:depthTargetDescriptor];
    appData->drawableRenderDescriptor.depthAttachment.texture = appData->depthTarget;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* appData)
{
    pl_handle_extension_reloads();

    pl_new_io_frame();

    plIOContext* ptIOCtx = pl_get_io_context();
    appData->graphics.metalLayer = ptIOCtx->pBackendRendererData;

    appData->graphics.currentFrame++;

    // begin profiling frame
    pl_begin_profile_frame(appData->graphics.currentFrame);

    // request command buffer
    id<MTLCommandBuffer> commandBuffer = [appData->graphics.cmdQueue commandBuffer];

    // get next drawable
    id<CAMetalDrawable> currentDrawable = [appData->graphics.metalLayer nextDrawable];

    if(!currentDrawable)
        return;

    // set colorattachment to next drawable
    appData->drawableRenderDescriptor.colorAttachments[0].texture = currentDrawable.texture;

    pl_new_draw_frame_metal(&appData->ctx, appData->drawableRenderDescriptor);

    // create render command encoder
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:appData->drawableRenderDescriptor];

    appData->ptDrawExtApi->pl_add_text(appData->fgDrawLayer, &appData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "extension baby");

    // draw profiling info
    pl_begin_profile_sample("Draw Profiling Info");

    char cPProfileValue[64] = {0};
    for(uint32_t i = 0u; i < pl_sb_size(appData->tProfileCtx.ptLastFrame->sbtSamples); i++)
    {
        plProfileSample* tPSample = &appData->tProfileCtx.ptLastFrame->sbtSamples[i];
        pl_add_text(appData->fgDrawLayer, &appData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, tPSample->pcName, 0.0f);
        plVec2 sampleTextSize = pl_calculate_text_size(&appData->fontAtlas.sbFonts[0], 13.0f, tPSample->pcName, 0.0f);
        pl_sprintf(cPProfileValue, ": %0.5f", tPSample->dDuration);
        pl_add_text(appData->fgDrawLayer, &appData->fontAtlas.sbFonts[0], 13.0f, (plVec2){sampleTextSize.x + 15.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, cPProfileValue, 0.0f);
    }
    pl_end_profile_sample();

    // draw commands
    pl_begin_profile_sample("Add draw commands");
    pl_add_text(appData->fgDrawLayer, &appData->fontAtlas.sbFonts[0], 13.0f, (plVec2){300.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(appData->bgDrawLayer, (plVec2){300.0f, 50.0f}, (plVec2){300.0f, 150.0f}, (plVec2){350.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl__begin_profile_sample("Calculate text size");
    plVec2 textSize = pl_calculate_text_size(&appData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl__end_profile_sample();
    pl_add_rect_filled(appData->bgDrawLayer, (plVec2){300.0f, 10.0f}, (plVec2){300.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(appData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    pl_end_profile_sample();

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    pl_submit_draw_layer(appData->bgDrawLayer);
    pl_submit_draw_layer(appData->fgDrawLayer);
    pl_end_profile_sample();

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    pl_submit_drawlist_metal(&appData->drawlist, ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    pl_end_profile_sample();

    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];

    pl_end_io_frame();

    // end profiling frame
    pl_end_profile_frame();
}