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

#include "metal_pl.h"
#include "metal_pl_graphics.h"
#include "pl_ds.h"
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

    // draw commands
    pl_add_text(userData->fgDrawLayer, &userData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(userData->bgDrawLayer, (plVec2){10.0f, 50.0f}, (plVec2){10.0f, 150.0f}, (plVec2){150.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    plVec2 textSize = pl_calculate_text_size(&userData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl_add_rect_filled(userData->bgDrawLayer, (plVec2){10.0f, 10.0f}, (plVec2){10.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(userData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    
    // submit draw layers
    pl_submit_draw_layer(userData->bgDrawLayer);
    pl_submit_draw_layer(userData->fgDrawLayer);

    // submit draw lists
    pl_submit_drawlist_metal(userData->drawlist, appData->clientWidth, appData->clientHeight, renderEncoder);

    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];
}