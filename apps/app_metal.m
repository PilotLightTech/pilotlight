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
    plProfileContext*        ptProfileCtx;
    plLogContext*            ptLogCtx;
    plUiContext*             ptUiContext;

    // apis
    plFileApiI*             ptFileApi;

    // extension apis
    plDrawApiI*             ptDrawApi;
    plUiApiI*               ptUiApi;
    plMetalDrawApiI*        ptMetalDrawApi;

    plApiRegistryApiI* ptApiRegistry;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data("memory"));
    pl_set_io_context(ptDataRegistry->get_data(PL_CONTEXT_IO_NAME));

    if(ptAppData) // reload
    {
        pl_set_log_context(ptDataRegistry->get_data("log"));
        pl_set_profile_context(ptDataRegistry->get_data("profile"));
        
        ptAppData->ptDrawApi = ptApiRegistry->first(PL_API_DRAW);
        ptAppData->ptUiApi      = ptApiRegistry->first(PL_API_UI);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    ptAppData->ptApiRegistry = ptApiRegistry;
    ptDataRegistry->set_data("profile", ptProfileCtx);
    ptDataRegistry->set_data("log", ptLogCtx);

    plIOContext* ptIOCtx = pl_get_io_context();
    ptAppData->device.device = ptIOCtx->pBackendPlatformData;
    
    // set contexts

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load(ptApiRegistry, "pl_image_ext", "pl_load_image_ext", "pl_unload_image_ext", false);
    ptExtensionRegistry->load(ptApiRegistry, "pl_draw_ext", "pl_load_draw_ext", "pl_unload_draw_ext", true);
    ptExtensionRegistry->load(ptApiRegistry, "pl_ui_ext", "pl_load_ui_ext", "pl_unload_ui_ext", true);

    plImageApiI* ptImageApi = ptApiRegistry->first(PL_API_IMAGE);
    plDrawApiI* ptDrawApi = ptApiRegistry->first(PL_API_DRAW);
    plMetalDrawApiI* ptMetalApi = ptApiRegistry->first(PL_API_METAL_DRAW);
    plUiApiI* ptUi = ptApiRegistry->first(PL_API_UI);
    ptAppData->ptDrawApi = ptDrawApi;
    ptAppData->ptMetalDrawApi = ptMetalApi;
    ptAppData->ptUiApi = ptUi;
    plUiContext* ptUiContext  = ptUi->create_context();
    ptDataRegistry->set_data(PL_CONTEXT_DRAW_NAME, ptDrawApi->get_context());
    ptDataRegistry->set_data("ui", ptUiContext);
    ptDataRegistry->set_data("device", &ptAppData->device);

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
    ptMetalApi->initialize_context(ptAppData->device.device);

    // create draw list & layers
    plDrawContext* ptDrawCtx = ptDrawApi->get_context();
    ptDrawApi->register_drawlist(ptDrawCtx, &ptAppData->drawlist);
    ptAppData->bgDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = ptDrawApi->request_layer(&ptAppData->drawlist, "Foreground Layer");
    
    // create font atlas
    ptDrawApi->add_default_font(&ptAppData->fontAtlas);
    ptDrawApi->build_font_atlas(ptDrawCtx, &ptAppData->fontAtlas);
    ptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plDrawApiI* ptDrawApi = ptAppData->ptDrawApi;
    plUiApiI* ptUi = ptAppData->ptUiApi;

    // clean up contexts
    ptDrawApi->cleanup_font_atlas(&ptAppData->fontAtlas);
    ptUi->destroy_context(NULL);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(void* pAppData)
{
    plAppData* ptAppData = pAppData;
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
pl_app_update(void* pAppData)
{
    plAppData* ptAppData = pAppData;
    plDrawApiI* ptDrawApi = ptAppData->ptDrawApi;
    plMetalDrawApiI* ptMetalApi = ptAppData->ptMetalDrawApi;
    plUiApiI* ptUi = ptAppData->ptUiApi;

    ptMetalApi->new_frame(ptDrawApi->get_context(), ptAppData->drawableRenderDescriptor);
    ptUi->new_frame();

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

    // draw profiling info
    pl_begin_profile_sample("Draw Profiling Info");

    char cPProfileValue[64] = {0};
    uint32_t uSampleCount = 0;
    plProfileSample* ptSamples = pl_get_last_frame_samples(&uSampleCount);
    for(uint32_t i = 0u; i < uSampleCount; i++)
    {
        plProfileSample* tPSample = &ptSamples[i];
        ptDrawApi->add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, tPSample->pcName, 0.0f);
        plVec2 sampleTextSize = ptDrawApi->calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, tPSample->pcName, 0.0f);
        pl_sprintf(cPProfileValue, ": %0.5f", tPSample->dDuration);
        ptDrawApi->add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){sampleTextSize.x + 15.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, cPProfileValue, 0.0f);
    }
    pl_end_profile_sample();

    // draw commands
    pl_begin_profile_sample("Add draw commands");
    ptDrawApi->add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){300.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    ptDrawApi->add_triangle_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 50.0f}, (plVec2){300.0f, 150.0f}, (plVec2){350.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl__begin_profile_sample("Calculate text size");
    plVec2 textSize = ptDrawApi->calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl__end_profile_sample();
    ptDrawApi->add_rect_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 10.0f}, (plVec2){300.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    ptDrawApi->add_line(ptAppData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    pl_end_profile_sample();

    static bool bOpen = true;


    if(ptUi->begin_window("Pilot Light", NULL, false))
    {
        ptUi->text("%.6f ms", ptIOCtx->fDeltaTime);

        ptUi->checkbox("Camera Info", &bOpen);
        ptUi->end_window();
    }

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    ptDrawApi->submit_layer(ptAppData->bgDrawLayer);
    ptDrawApi->submit_layer(ptAppData->fgDrawLayer);
    pl_end_profile_sample();

    ptUi->render();

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    ptDrawApi->get_context()->tFrameBufferScale.x = ptIOCtx->afMainFramebufferScale[0];
    ptDrawApi->get_context()->tFrameBufferScale.y = ptIOCtx->afMainFramebufferScale[1];
    ptMetalApi->submit_drawlist(&ptAppData->drawlist, ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    ptMetalApi->submit_drawlist(ptUi->get_draw_list(NULL), ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    ptMetalApi->submit_drawlist(ptUi->get_debug_draw_list(NULL), ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], renderEncoder);
    pl_end_profile_sample();

    // finish recording
    [renderEncoder endEncoding];

    // present
    [commandBuffer presentDrawable:currentDrawable];

    // submit command buffer
    [commandBuffer commit];
    
    // end profiling frame
    pl_end_profile_frame();
}