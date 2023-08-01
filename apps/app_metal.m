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

#include <stdio.h>

#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_os.h"
#include "pl_memory.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "pl_metal.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_image_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_stats_ext.h"

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
    plDrawList               drawlist;
    plDrawLayer*             fgDrawLayer;
    plDrawLayer*             bgDrawLayer;
    plFontAtlas              fontAtlas;
    bool                     bShowUiDemo;
    bool                     bShowUiStyle;
    bool                     bShowUiDebug;

    // new
    id<MTLBuffer>              tIndexBuffer;
    id<MTLBuffer>              tVertexBuffer;
    id<MTLRenderPipelineState> tPipeline;
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plDrawApiI*              gptDraw              = NULL;
const plMetalDrawApiI*         gptMetalDraw         = NULL;
const plUiApiI*                gptUi                = NULL;
const plStatsApiI*             gptStats             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;
const plFileApiI*              gptFile              = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_io_context(gptDataRegistry->get_data(PL_CONTEXT_IO_NAME));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptDraw      = ptApiRegistry->first(PL_API_DRAW);
        gptMetalDraw = ptApiRegistry->first(PL_API_METAL_DRAW);
        gptUi        = ptApiRegistry->first(PL_API_UI);
        gptStats     = ptApiRegistry->first(PL_API_STATS);
        gptFile      = ptApiRegistry->first(PL_API_FILE);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    gptDataRegistry->set_data("profile", ptProfileCtx);
    gptDataRegistry->set_data("log", ptLogCtx);

    plIOContext* ptIOCtx = pl_get_io_context();
    ptAppData->device.device = ptIOCtx->pBackendPlatformData;

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    const plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_image_ext", "pl_load_image_ext", "pl_unload_image_ext", false);
    ptExtensionRegistry->load("pl_draw_ext", "pl_load_draw_ext", "pl_unload_draw_ext", true);
    ptExtensionRegistry->load("pl_ui_ext", "pl_load_ui_ext", "pl_unload_ui_ext", true);
    ptExtensionRegistry->load("pl_stats_ext", "pl_load_stats_ext", "pl_unload_stats_ext", false);

    // load apis
    gptDraw      = ptApiRegistry->first(PL_API_DRAW);
    gptMetalDraw = ptApiRegistry->first(PL_API_METAL_DRAW);
    gptUi        = ptApiRegistry->first(PL_API_UI);
    gptStats     = ptApiRegistry->first(PL_API_STATS);
    gptFile      = ptApiRegistry->first(PL_API_FILE);

    plUiContext* ptUiContext  = gptUi->create_context();
    gptDataRegistry->set_data("device", &ptAppData->device);

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

    // initialize backend specifics for draw context
    gptMetalDraw->initialize_context(ptAppData->device.device);

    // create draw list & layers
    plDrawContext* ptDrawCtx = gptDraw->get_context();
    gptDraw->register_drawlist(ptDrawCtx, &ptAppData->drawlist);
    ptAppData->bgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Foreground Layer");
    
    // create font atlas
    gptDraw->add_default_font(&ptAppData->fontAtlas);
    gptDraw->build_font_atlas(ptDrawCtx, &ptAppData->fontAtlas);
    gptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);

    // new demo

    // vertex buffer
    const float fVertexBuffer[] = {
        // x, y, z, r, g, b, a
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
         0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    };
    ptAppData->tVertexBuffer = [ptAppData->device.device newBufferWithLength:sizeof(float) * 21 options:MTLResourceStorageModeShared];
    memcpy(ptAppData->tVertexBuffer.contents, fVertexBuffer, sizeof(float) * 21);

    // index buffer
    const uint32_t uIndexBuffer[] = {
        0, 1, 2
    };
    ptAppData->tIndexBuffer = [ptAppData->device.device newBufferWithLength:sizeof(uint32_t) * 3 options:MTLResourceStorageModeShared];
    memcpy(ptAppData->tIndexBuffer.contents, uIndexBuffer, sizeof(uint32_t) * 3);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    // clean up contexts
    gptDraw->cleanup_font_atlas(&ptAppData->fontAtlas);
    gptUi->destroy_context(NULL);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    PL_FREE(ptAppData);
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

    static int bFirstRun = true;
    if(bFirstRun)
    {

        unsigned uShaderFileSize = 0;
        gptFile->read("../shaders/metal/primitive.metal", &uShaderFileSize, NULL, "rb");
        char* pcFileData = PL_ALLOC(uShaderFileSize);
        gptFile->read("../shaders/metal/primitive.metal", &uShaderFileSize, pcFileData, "rb");

        NSError* error = nil;
        NSString* shaderSource = [NSString stringWithUTF8String:pcFileData];
        id<MTLLibrary> library = [ptAppData->device.device newLibraryWithSource:shaderSource options:nil error:&error];
        if (library == nil)
        {
            NSLog(@"Error: failed to create Metal library: %@", error);
        }

        id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
        id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

        if (vertexFunction == nil || fragmentFunction == nil)
        {
            NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        }

        MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        vertexDescriptor.attributes[0].offset = 0;
        vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3; // position
        vertexDescriptor.attributes[0].bufferIndex = 0;
        vertexDescriptor.attributes[1].offset = sizeof(float) * 3;
        vertexDescriptor.attributes[1].format = MTLVertexFormatFloat4; // color
        vertexDescriptor.attributes[1].bufferIndex = 0;
        vertexDescriptor.layouts[0].stepRate = 1;
        vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        vertexDescriptor.layouts[0].stride = sizeof(float) * 7;

        MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
        depthDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
        depthDescriptor.depthWriteEnabled = NO;
        ptAppData->tDepthStencilState = [ptAppData->device.device newDepthStencilStateWithDescriptor:depthDescriptor];

        MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = vertexFunction;
        pipelineDescriptor.fragmentFunction = fragmentFunction;
        pipelineDescriptor.vertexDescriptor = vertexDescriptor;
        pipelineDescriptor.rasterSampleCount = 1;
        pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
        pipelineDescriptor.depthAttachmentPixelFormat = ptAppData->drawableRenderDescriptor.depthAttachment.texture.pixelFormat;
        pipelineDescriptor.stencilAttachmentPixelFormat = ptAppData->drawableRenderDescriptor.stencilAttachment.texture.pixelFormat;

        ptAppData->tRenderPipelineState = [ptAppData->device.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
        if (error != nil)
            NSLog(@"Error: failed to create Metal pipeline state: %@", error);
        bFirstRun = false;
    }

    gptMetalDraw->new_frame(gptDraw->get_context(), ptAppData->drawableRenderDescriptor);
    gptUi->new_frame();

    plIOContext* ptIOCtx = pl_get_io_context();
    ptAppData->graphics.metalLayer = ptIOCtx->pBackendRendererData;

    ptAppData->graphics.currentFrame++;

    // begin profiling frame
    pl_begin_profile_frame();

    // request command buffer
    id<MTLCommandBuffer> commandBuffer = [ptAppData->graphics.cmdQueue commandBuffer];

    // get next drawable
    id<CAMetalDrawable> currentDrawable = [ptAppData->graphics.metalLayer nextDrawable];

    if(!currentDrawable)
        return;

    // set colorattachment to next drawable
    ptAppData->drawableRenderDescriptor.colorAttachments[0].texture = currentDrawable.texture;

    // create render command encoder
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:ptAppData->drawableRenderDescriptor];

    if(gptUi->begin_window("Pilot Light", NULL, false))
    {
        if(gptUi->collapsing_header("User Interface"))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }
        gptUi->end_window();
    }

    if(ptAppData->bShowUiDemo)
    {
        pl_begin_profile_sample("ui demo");
        gptUi->demo(&ptAppData->bShowUiDemo);
        pl_end_profile_sample();
    }

    if(ptAppData->bShowUiStyle)
        gptUi->style(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUi->debug(&ptAppData->bShowUiDebug);

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    gptDraw->submit_layer(ptAppData->bgDrawLayer);
    gptDraw->submit_layer(ptAppData->fgDrawLayer);
    pl_end_profile_sample();

    gptUi->render();

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    gptDraw->get_context()->tFrameBufferScale.x = ptIOCtx->afMainFramebufferScale[0];
    gptDraw->get_context()->tFrameBufferScale.y = ptIOCtx->afMainFramebufferScale[1];
    gptMetalDraw->submit_drawlist(&ptAppData->drawlist, ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    gptMetalDraw->submit_drawlist(gptUi->get_draw_list(NULL), ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    gptMetalDraw->submit_drawlist(gptUi->get_debug_draw_list(NULL), ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    pl_end_profile_sample();

    // new
    // [renderEncoder setVertexBytes:&ortho_projection length:sizeof(ortho_projection) atIndex:1 ];
    [renderEncoder setDepthStencilState:ptAppData->tDepthStencilState];
    [renderEncoder setVertexBuffer:ptAppData->tVertexBuffer offset:0 atIndex:0];
    [renderEncoder setRenderPipelineState:ptAppData->tRenderPipelineState];
    [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:3 indexType:MTLIndexTypeUInt32 indexBuffer:ptAppData->tIndexBuffer indexBufferOffset:0];


    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];
    
    // end profiling frame
    pl_end_profile_frame();
}