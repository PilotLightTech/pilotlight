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
// [SECTION] pl_app_render
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "metal_pl.h"
#include "metal_pl_graphics.h"
#include "pl_ds.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "metal_pl_drawing.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plUserData_t
{
    id<MTLTexture>              depthTarget;
    MTLRenderPassDescriptor*    drawableRenderDescriptor;
    plDrawContext*              ctx;
    plDrawList*                 drawlist;
    plDrawLayer*                fgDrawLayer;
    plDrawLayer*                bgDrawLayer;
    plFontAtlas                 fontAtlas;
    plProfileContext            tProfileCtx;
    plLogContext                tLogCtx;
} plUserData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plAppData* appData, plUserData* userData)
{
    if(userData)
        return userData;
    plUserData* newUserData = malloc(sizeof(plUserData));
    memset(newUserData, 0, sizeof(plUserData));
    return newUserData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_setup(plAppData* appData, plUserData* userData)
{
    // setup profiling
    pl_create_profile_context(&userData->tProfileCtx);

    // setup logging
    pl_create_log_context(&userData->tLogCtx);
    pl_add_log_channel(&userData->tLogCtx, "Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(&userData->tLogCtx, 0, "Setup logging");

    // create command queue
    appData->graphics.cmdQueue = [appData->device.device newCommandQueue];

    // render pass descriptor
    userData->drawableRenderDescriptor = [MTLRenderPassDescriptor new];

    // color attachment
    userData->drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    userData->drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    userData->drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.01, 0, 0, 1);

    // depth attachment
    userData->drawableRenderDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    userData->drawableRenderDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    userData->drawableRenderDescriptor.depthAttachment.clearDepth = 1.0;

    // create draw context
    userData->ctx = pl_create_draw_context_metal(appData->device.device);

    // create draw list & layers
    userData->drawlist = pl_create_drawlist(userData->ctx);
    userData->bgDrawLayer = pl_request_draw_layer(userData->drawlist, "Background Layer");
    userData->fgDrawLayer = pl_request_draw_layer(userData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&userData->fontAtlas);
    pl_build_font_atlas(userData->ctx, &userData->fontAtlas);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* appData, plUserData* userData)
{
    pl_cleanup_font_atlas(&userData->fontAtlas);
    pl_cleanup_draw_context(userData->ctx);
    pl_cleanup_profile_context(&userData->tProfileCtx);
    pl_cleanup_log_context(&userData->tLogCtx);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* appData, plUserData* userData)
{    
    // recreate depth texture
    MTLTextureDescriptor *depthTargetDescriptor = [MTLTextureDescriptor new];
    depthTargetDescriptor.width       = appData->clientWidth;
    depthTargetDescriptor.height      = appData->clientHeight;
    depthTargetDescriptor.pixelFormat = MTLPixelFormatDepth32Float;
    depthTargetDescriptor.storageMode = MTLStorageModePrivate;
    depthTargetDescriptor.usage       = MTLTextureUsageRenderTarget;
    userData->depthTarget = [appData->device.device newTextureWithDescriptor:depthTargetDescriptor];
    userData->drawableRenderDescriptor.depthAttachment.texture = userData->depthTarget;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_render
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_render(plAppData* appData, plUserData* userData)
{
    appData->graphics.currentFrame++;

    // begin profiling frame
    pl_begin_profile_frame(&userData->tProfileCtx, appData->graphics.currentFrame);

    // request command buffer
    id<MTLCommandBuffer> commandBuffer = [appData->graphics.cmdQueue commandBuffer];

    // get next drawable
    id<CAMetalDrawable> currentDrawable = [appData->graphics.metalLayer nextDrawable];

    if(!currentDrawable)
        return;

    // set colorattachment to next drawable
    userData->drawableRenderDescriptor.colorAttachments[0].texture = currentDrawable.texture;

    pl_new_draw_frame_metal(userData->ctx, userData->drawableRenderDescriptor);

    // create render command encoder
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:userData->drawableRenderDescriptor];

    // draw profiling info
    pl_begin_profile_sample(&userData->tProfileCtx, "Draw Profiling Info");
    char cPProfileValue[64] = {0};
    for(uint32_t i = 0u; i < pl_sb_size(userData->tProfileCtx.tPLastFrame->sbSamples); i++)
    {
        plProfileSample* tPSample = &userData->tProfileCtx.tPLastFrame->sbSamples[i];
        pl_add_text(userData->fgDrawLayer, &userData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f + (float)tPSample->uDepth * 15.0f, 10.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, tPSample->cPName, 0.0f);
        plVec2 sampleTextSize = pl_calculate_text_size(&userData->fontAtlas.sbFonts[0], 13.0f, tPSample->cPName, 0.0f);
        pl_sprintf(cPProfileValue, ": %0.5f", tPSample->dDuration);
        pl_add_text(userData->fgDrawLayer, &userData->fontAtlas.sbFonts[0], 13.0f, (plVec2){sampleTextSize.x + 15.0f + (float)tPSample->uDepth * 15.0f, 10.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, cPProfileValue, 0.0f);
    }
    pl_end_profile_sample(&userData->tProfileCtx);

    // draw commands
    pl_begin_profile_sample(&userData->tProfileCtx, "Add draw commands");
    pl_add_text(userData->fgDrawLayer, &userData->fontAtlas.sbFonts[0], 13.0f, (plVec2){300.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(userData->bgDrawLayer, (plVec2){300.0f, 50.0f}, (plVec2){300.0f, 150.0f}, (plVec2){350.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl__begin_profile_sample(&userData->tProfileCtx, "Calculate text size");
    plVec2 textSize = pl_calculate_text_size(&userData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl__end_profile_sample(&userData->tProfileCtx);
    pl_add_rect_filled(userData->bgDrawLayer, (plVec2){300.0f, 10.0f}, (plVec2){300.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(userData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    pl_end_profile_sample(&userData->tProfileCtx);

    // submit draw layers
    pl_begin_profile_sample(&userData->tProfileCtx, "Submit draw layers");
    pl_submit_draw_layer(userData->bgDrawLayer);
    pl_submit_draw_layer(userData->fgDrawLayer);
    pl_end_profile_sample(&userData->tProfileCtx);

    // submit draw lists
    pl_begin_profile_sample(&userData->tProfileCtx, "Submit draw lists");
    pl_submit_drawlist_metal(userData->drawlist, appData->clientWidth, appData->clientHeight, renderEncoder);
    pl_end_profile_sample(&userData->tProfileCtx);

    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];

    // end profiling frame
    pl_end_profile_frame(&userData->tProfileCtx);
}