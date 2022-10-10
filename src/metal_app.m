/*
   metal_app.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] globals
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
#include "metal_pl_drawing.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct
{
    uint32_t                    viewportSize[2]; // required by apple_pl.m
    id<MTLTexture>              depthTarget;
    MTLRenderPassDescriptor*    drawableRenderDescriptor;
    plDrawContext               ctx;
    plDrawList*                 drawlist;
    plDrawLayer*                fgDrawLayer;
    plDrawLayer*                bgDrawLayer;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

static plAppData gAppData = {0};

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

void
pl_app_setup()
{
    // create command queue
    gGraphics.cmdQueue = [gDevice.device newCommandQueue];

    // render pass descriptor
    gAppData.drawableRenderDescriptor = [MTLRenderPassDescriptor new];

    // color attachment
    gAppData.drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    gAppData.drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    gAppData.drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 1, 1, 1);

    // depth attachment
    gAppData.drawableRenderDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    gAppData.drawableRenderDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    gAppData.drawableRenderDescriptor.depthAttachment.clearDepth = 1.0;

    // create draw context
    pl_setup_draw_context_metal(&gAppData.ctx, gDevice.device);

    // create draw list & layers
    gAppData.drawlist = malloc(sizeof(plDrawList));
    pl_create_drawlist(&gAppData.ctx, gAppData.drawlist);
    gAppData.bgDrawLayer = pl_request_draw_layer(gAppData.drawlist, "Background Layer");
    gAppData.fgDrawLayer = pl_request_draw_layer(gAppData.drawlist, "Foreground Layer");
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

void
pl_app_shutdown()
{
    // not actually called yet
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

void
pl_app_resize()
{    
    // recreate depth texture
    MTLTextureDescriptor *depthTargetDescriptor = [MTLTextureDescriptor new];
    depthTargetDescriptor.width       = gAppData.viewportSize[0];
    depthTargetDescriptor.height      = gAppData.viewportSize[1];
    depthTargetDescriptor.pixelFormat = MTLPixelFormatDepth32Float;
    depthTargetDescriptor.storageMode = MTLStorageModePrivate;
    depthTargetDescriptor.usage       = MTLTextureUsageRenderTarget;
    gAppData.depthTarget = [gDevice.device newTextureWithDescriptor:depthTargetDescriptor];
    gAppData.drawableRenderDescriptor.depthAttachment.texture = gAppData.depthTarget;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_render
//-----------------------------------------------------------------------------

void
pl_app_render()
{
    gGraphics.currentFrame++;

    // request command buffer
    id<MTLCommandBuffer> commandBuffer = [gGraphics.cmdQueue commandBuffer];

    // get next drawable
    id<CAMetalDrawable> currentDrawable = [gGraphics.metalLayer nextDrawable];

    if(!currentDrawable)
        return;

    // set colorattachment to next drawable
    gAppData.drawableRenderDescriptor.colorAttachments[0].texture = currentDrawable.texture;

    pl_new_draw_frame_metal(&gAppData.ctx, gAppData.drawableRenderDescriptor);

    // create render command encoder
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:gAppData.drawableRenderDescriptor];

    // draw commands
    pl_add_triangle_filled(gAppData.bgDrawLayer, (plVec2){10.0f, 10.0f}, (plVec2){10.0f, 200.0f}, (plVec2){200.0f, 200.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl_add_triangle_filled(gAppData.bgDrawLayer, (plVec2){10.0f, 10.0f}, (plVec2){200.0f, 200.0f}, (plVec2){200.0f, 10.0f}, (plVec4){0.0f, 1.0f, 0.0f, 1.0f});

    // submit draw layers
    pl_submit_draw_layer(gAppData.bgDrawLayer);
    pl_submit_draw_layer(gAppData.fgDrawLayer);

    // submit draw lists
    pl_submit_drawlist_metal(gAppData.drawlist, gAppData.viewportSize[0], gAppData.viewportSize[1], renderEncoder);

    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];
}