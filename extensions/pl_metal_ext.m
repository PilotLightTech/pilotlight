/*
   pl_metal_ext.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] internal structs & types
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_os.h"
#include "pl_graphics_ext.c"

// pilotlight ui
#include "pl_ui.h"
#include "pl_ui_metal.h"

// metal stuff
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

const plFileApiI* gptFile= NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal structs & types
//-----------------------------------------------------------------------------

@interface plMetalBuffer : NSObject
@property (nonatomic, strong) id<MTLBuffer> buffer;
@property (nonatomic, assign) double        lastReuseTime;
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer;
@end

@implementation plMetalBuffer
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer
{
    if ((self = [super init]))
    {
        _buffer = buffer;
        _lastReuseTime = pl_get_io()->dTime;
    }
    return self;
}
@end

typedef struct _plMetalPipelineEntry
{
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tSolidRenderPipelineState;
    id<MTLRenderPipelineState> tLineRenderPipelineState;
    pl3DDrawFlags              tFlags;
} plMetalPipelineEntry;

typedef struct _plGraphicsMetal
{
    id<MTLCommandQueue> tCmdQueue;
    uint32_t            uCurrentFrame;
    CAMetalLayer*       pMetalLayer;

    // temp
    id<MTLTexture>             depthTarget;
    MTLRenderPassDescriptor*   drawableRenderDescriptor;
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;

    // drawing
    plMetalPipelineEntry* sbtPipelineEntries;
    id<MTLFunction> tSolidVertexFunction;
    id<MTLFunction> tLineVertexFunction;
    id<MTLFunction> tFragmentFunction;
    id<MTLBuffer>   tIndexBuffer;
    id<MTLBuffer>   tVertexBuffer;
    NSMutableArray<plMetalBuffer*>* bufferCache;
    double                        lastBufferCachePurge;

    // per frame
    id<CAMetalDrawable>         tCurrentDrawable;
    id<MTLCommandBuffer>        tCurrentCommandBuffer;
    id<MTLRenderCommandEncoder> tCurrentRenderEncoder;

} plGraphicsMetal;

typedef struct _plDeviceMetal
{
    id<MTLDevice> tDevice;
} plDeviceMetal;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

plMetalBuffer*
pl__dequeue_reusable_buffer(plGraphics* ptGraphics, NSUInteger length)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    uint64_t now = pl_get_io()->dTime;

    @synchronized(ptMetalGraphics->bufferCache)
    {
        // Purge old buffers that haven't been useful for a while
        if (now - ptMetalGraphics->lastBufferCachePurge > 1.0)
        {
            NSMutableArray* survivors = [NSMutableArray array];
            for (plMetalBuffer* candidate in ptMetalGraphics->bufferCache)
                if (candidate.lastReuseTime > ptMetalGraphics->lastBufferCachePurge)
                    [survivors addObject:candidate];
            ptMetalGraphics->bufferCache = [survivors mutableCopy];
            ptMetalGraphics->lastBufferCachePurge = now;
        }

        // see if we have a buffer we can reuse
        plMetalBuffer* bestCandidate = nil;
        for (plMetalBuffer* candidate in ptMetalGraphics->bufferCache)
            if (candidate.buffer.length >= length && (bestCandidate == nil || bestCandidate.lastReuseTime > candidate.lastReuseTime))
                bestCandidate = candidate;

        if (bestCandidate != nil)
        {
            [ptMetalGraphics->bufferCache removeObject:bestCandidate];
            bestCandidate.lastReuseTime = now;
            return bestCandidate;
        }
    }

    // make a new buffer
    id<MTLBuffer> backing = [ptMetalDevice->tDevice newBufferWithLength:length options:MTLResourceStorageModeShared];
    return [[plMetalBuffer alloc] initWithBuffer:backing];
}

static plMetalPipelineEntry*
pl__get_3d_pipelines(plGraphics* ptGraphics, pl3DDrawFlags tFlags)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    for(uint32_t i = 0; i < pl_sb_size(ptMetalGraphics->sbtPipelineEntries); i++)
    {
        if(ptMetalGraphics->sbtPipelineEntries[i].tFlags == tFlags)
            return &ptMetalGraphics->sbtPipelineEntries[i];
    }

    // pipeline not found, make new one

    plMetalPipelineEntry tPipelineEntry = {
        .tFlags = tFlags
    };

    NSError* error = nil;

    // line rendering
    {
        MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        vertexDescriptor.attributes[0].offset = 0;
        vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3; // position
        vertexDescriptor.attributes[0].bufferIndex = 0;

        vertexDescriptor.attributes[1].offset = sizeof(float) * 3;
        vertexDescriptor.attributes[1].format = MTLVertexFormatFloat3; // info
        vertexDescriptor.attributes[1].bufferIndex = 0;

        vertexDescriptor.attributes[2].offset = sizeof(float) * 6;
        vertexDescriptor.attributes[2].format = MTLVertexFormatFloat3; // other position
        vertexDescriptor.attributes[3].bufferIndex = 0;

        vertexDescriptor.attributes[3].offset = sizeof(float) * 9;
        vertexDescriptor.attributes[3].format = MTLVertexFormatUChar4; // color
        vertexDescriptor.attributes[3].bufferIndex = 0;

        vertexDescriptor.layouts[0].stepRate = 1;
        vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        vertexDescriptor.layouts[0].stride = sizeof(float) * 10;

        MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
        depthDescriptor.depthCompareFunction = (tFlags & PL_PIPELINE_FLAG_DEPTH_TEST) ? MTLCompareFunctionLessEqual : MTLCompareFunctionAlways;
        depthDescriptor.depthWriteEnabled = (tFlags & PL_PIPELINE_FLAG_DEPTH_WRITE) ? YES : NO;
        tPipelineEntry.tDepthStencilState = [ptMetalDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor];

        MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = ptMetalGraphics->tLineVertexFunction;
        pipelineDescriptor.fragmentFunction = ptMetalGraphics->tFragmentFunction;
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
        pipelineDescriptor.depthAttachmentPixelFormat = ptMetalGraphics->drawableRenderDescriptor.depthAttachment.texture.pixelFormat;
        pipelineDescriptor.stencilAttachmentPixelFormat = ptMetalGraphics->drawableRenderDescriptor.stencilAttachment.texture.pixelFormat;

        tPipelineEntry.tLineRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];

        if (error != nil)
            NSLog(@"Error: failed to create Metal pipeline state: %@", error);
    }

    // solid rendering
    {
        MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        vertexDescriptor.attributes[0].offset = 0;
        vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3; // position
        vertexDescriptor.attributes[0].bufferIndex = 0;
        vertexDescriptor.attributes[1].offset = sizeof(float) * 3;
        vertexDescriptor.attributes[1].format = MTLVertexFormatUChar4; // color
        vertexDescriptor.attributes[1].bufferIndex = 0;
        vertexDescriptor.layouts[0].stepRate = 1;
        vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        vertexDescriptor.layouts[0].stride = sizeof(float) * 4;

        MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = ptMetalGraphics->tSolidVertexFunction;
        pipelineDescriptor.fragmentFunction = ptMetalGraphics->tFragmentFunction;
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
        pipelineDescriptor.depthAttachmentPixelFormat = ptMetalGraphics->drawableRenderDescriptor.depthAttachment.texture.pixelFormat;
        pipelineDescriptor.stencilAttachmentPixelFormat = ptMetalGraphics->drawableRenderDescriptor.stencilAttachment.texture.pixelFormat;

        tPipelineEntry.tSolidRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
        if (error != nil)
            NSLog(@"Error: failed to create Metal pipeline state: %@", error);
    }

    pl_sb_push(ptMetalGraphics->sbtPipelineEntries, tPipelineEntry);
    return &ptMetalGraphics->sbtPipelineEntries[pl_sb_size(ptMetalGraphics->sbtPipelineEntries) - 1];
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static uint32_t
pl_create_index_buffer(plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    id<MTLBuffer> tVertexBuffer = [ptMetalDevice->tDevice newBufferWithLength:szSize options:MTLResourceStorageModeShared];
    memcpy(tVertexBuffer.contents, pData, szSize);
    
    const uint32_t uBufferIndex = pl_sb_size(ptDevice->sbtBuffers);

    plBuffer tBuffer = {
        .pBuffer = tVertexBuffer
    };
    pl_sb_push(ptDevice->sbtBuffers, tBuffer);

    return uBufferIndex;
}

static uint32_t
pl_create_vertex_buffer(plDevice* ptDevice, size_t szSize, size_t szStride, const void* pData, const char* pcName)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    id<MTLBuffer> tVertexBuffer = [ptMetalDevice->tDevice newBufferWithLength:szSize options:MTLResourceStorageModeShared];
    memcpy(tVertexBuffer.contents, pData, szSize);
    
    const uint32_t uBufferIndex = pl_sb_size(ptDevice->sbtBuffers);

    plBuffer tBuffer = {
        .pBuffer = tVertexBuffer
    };
    pl_sb_push(ptDevice->sbtBuffers, tBuffer);

    return uBufferIndex;
}

static void
pl_initialize_graphics(plGraphics* ptGraphics)
{
    plIO* ptIOCtx = pl_get_io();

    ptGraphics->_pInternalData = PL_ALLOC(sizeof(plGraphicsMetal));
    memset(ptGraphics->_pInternalData, 0, sizeof(plGraphicsMetal));

    ptGraphics->tDevice._pInternalData = PL_ALLOC(sizeof(plDeviceMetal));
    memset(ptGraphics->tDevice._pInternalData, 0, sizeof(plDeviceMetal));

    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    ptMetalDevice->tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    // create command queue
    ptMetalGraphics->tCmdQueue = [ptMetalDevice->tDevice newCommandQueue];

    pl_initialize_metal(ptMetalDevice->tDevice);

    // setup
    // render pass descriptor
    ptMetalGraphics->drawableRenderDescriptor = [MTLRenderPassDescriptor new];

    // color attachment
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

    // depth attachment
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.clearDepth = 1.0;

    // temp
    unsigned uShaderFileSize = 0;
    gptFile->read("../shaders/metal/primitive.metal", &uShaderFileSize, NULL, "rb");
    char* pcFileData = PL_ALLOC(uShaderFileSize);
    gptFile->read("../shaders/metal/primitive.metal", &uShaderFileSize, pcFileData, "rb");

    NSError* error = nil;
    NSString* shaderSource = [NSString stringWithUTF8String:pcFileData];
    id<MTLLibrary> library = [ptMetalDevice->tDevice  newLibraryWithSource:shaderSource options:nil error:&error];
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
    ptMetalGraphics->tDepthStencilState = [ptMetalDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor];

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
    pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    pipelineDescriptor.stencilAttachmentPixelFormat = ptMetalGraphics->drawableRenderDescriptor.stencilAttachment.texture.pixelFormat;

    ptMetalGraphics->tRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    // line rendering
    {
        NSError* error = nil;

        NSString* lineShaderSource = @""
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "struct Uniforms {\n"
        "    float4x4 projectionMatrix;\n"
        "    float    fAspect;\n"
        "};\n"
        "\n"
        "struct VertexIn {\n"
        "    float3 aPos      [[attribute(0)]];\n"
        "    float3 aInfo     [[attribute(1)]];\n"
        "    float3 aPosOther [[attribute(2)]];\n"
        "    uchar4 color     [[attribute(3)]];\n"
        "};\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "    float4 color;\n"
        "};\n"
        "\n"
        "vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],\n"
        "                             constant Uniforms &uniforms [[buffer(1)]]) {\n"
        "    // clip space\n"
        "    float4 tCurrentProj = uniforms.projectionMatrix * float4(in.aPos.xyz, 1.0f);\n"
        "    float4 tOtherProj = uniforms.projectionMatrix * float4(in.aPosOther.xyz, 1.0f);\n"
        "    // NDC Space\n"
        "    float2 tCurrentNDC =  tCurrentProj.xy / tCurrentProj.w;\n"
        "    float2 tOtherNDC =  tOtherProj.xy / tOtherProj.w;\n"
        "    // correct for aspect\n"
        "    tCurrentNDC.x *= uniforms.fAspect;\n"
        "    tOtherNDC.x *= uniforms.fAspect;\n"
        "    // normal of line (B - A)\n"
        "    float2 dir = in.aInfo.z * normalize(tOtherNDC - tCurrentNDC);\n"
        "    float2 normal = float2(-dir.y, dir.x);\n"
        "    // extrude from center & correct aspect ratio\n"
        "    normal *= in.aInfo.y / 2.0;\n"
        "    normal.x /= uniforms.fAspect;\n"
        "    // offset by the direction of this point in the pair (-1 or 1)\n"
        "    float4 offset = float4(normal * in.aInfo.x, 0.0, 0.0);\n"
        "    VertexOut out;\n"
        "    out.position = tCurrentProj + offset;\n"
        "    out.position.y *= -1;\n"
        "    out.color = float4(in.color) / float4(255.0);\n"
        "    return out;\n"
        "}\n"
        "\n"
        "fragment half4 fragment_main(VertexOut in [[stage_in]]) {\n"
        "    return half4(in.color);\n"
        "}\n";

        id<MTLLibrary> library = [ptMetalDevice->tDevice newLibraryWithSource:lineShaderSource options:nil error:&error];
        if (library == nil)
        {
            NSLog(@"Error: failed to create Metal library: %@", error);
        }

        ptMetalGraphics->tLineVertexFunction = [library newFunctionWithName:@"vertex_main"];
        ptMetalGraphics->tFragmentFunction = [library newFunctionWithName:@"fragment_main"];

        NSString* solidShaderSource = @""
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "struct Uniforms {\n"
        "    float4x4 projectionMatrix;\n"
        "};\n"
        "\n"
        "struct VertexIn {\n"
        "    float3 position  [[attribute(0)]];\n"
        "    uchar4 color     [[attribute(1)]];\n"
        "};\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "    float2 texCoords;\n"
        "    float4 color;\n"
        "};\n"
        "\n"
        "vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],\n"
        "                             constant Uniforms &uniforms [[buffer(1)]]) {\n"
        "    VertexOut out;\n"
        "    out.position = uniforms.projectionMatrix * float4(in.position, 1);\n"
        "    out.position.y *= -1;\n"
        "    out.color = float4(in.color) / float4(255.0);\n"
        "    return out;\n"
        "}\n"
        "\n"
        "fragment half4 fragment_main(VertexOut in [[stage_in]]) {\n"
        "    return half4(in.color);\n"
        "}\n";

        id<MTLLibrary> library1 = [ptMetalDevice->tDevice newLibraryWithSource:solidShaderSource options:nil error:&error];
        if (library1 == nil)
        {
            NSLog(@"Error: failed to create Metal library: %@", error);
        }

        ptMetalGraphics->tSolidVertexFunction = [library1 newFunctionWithName:@"vertex_main"];
    }
}

static void
pl_resize(plGraphics* ptGraphics)
{
    plIO* ptIOCtx = pl_get_io();

    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    // recreate depth texture
    MTLTextureDescriptor *depthTargetDescriptor = [MTLTextureDescriptor new];
    depthTargetDescriptor.width       = (uint32_t)ptIOCtx->afMainViewportSize[0];
    depthTargetDescriptor.height      = (uint32_t)ptIOCtx->afMainViewportSize[1];
    depthTargetDescriptor.pixelFormat = MTLPixelFormatDepth32Float;
    depthTargetDescriptor.storageMode = MTLStorageModePrivate;
    depthTargetDescriptor.usage       = MTLTextureUsageRenderTarget;
    ptMetalGraphics->depthTarget = [ptMetalDevice->tDevice newTextureWithDescriptor:depthTargetDescriptor];
    ptMetalGraphics->drawableRenderDescriptor.depthAttachment.texture = ptMetalGraphics->depthTarget;
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    plIO* ptIOCtx = pl_get_io();
    ptMetalGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;
    
    ptMetalGraphics->uCurrentFrame++;

        // get next drawable
    ptMetalGraphics->tCurrentDrawable = [ptMetalGraphics->pMetalLayer nextDrawable];

    if(!ptMetalGraphics->tCurrentDrawable)
        return false;

    // set color attachment to next drawable
    ptMetalGraphics->drawableRenderDescriptor.colorAttachments[0].texture = ptMetalGraphics->tCurrentDrawable.texture;

    ptMetalGraphics->tCurrentCommandBuffer = [ptMetalGraphics->tCmdQueue commandBuffer];

    // reset 3d drawlists
    for(uint32_t i = 0u; i < pl_sb_size(ptGraphics->sbt3DDrawlists); i++)
    {
        plDrawList3D* drawlist = ptGraphics->sbt3DDrawlists[i];

        pl_sb_reset(drawlist->sbtSolidVertexBuffer);
        pl_sb_reset(drawlist->sbtLineVertexBuffer);
        pl_sb_reset(drawlist->sbtSolidIndexBuffer);    
        pl_sb_reset(drawlist->sbtLineIndexBuffer);    
    }

    return true;
}

static void
pl_end_gfx_frame(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;

    [ptMetalGraphics->tCurrentCommandBuffer presentDrawable:ptMetalGraphics->tCurrentDrawable];
    [ptMetalGraphics->tCurrentCommandBuffer commit];
}

static void
pl_begin_recording(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    ptMetalGraphics->tCurrentRenderEncoder = [ptMetalGraphics->tCurrentCommandBuffer renderCommandEncoderWithDescriptor:ptMetalGraphics->drawableRenderDescriptor];

}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    [ptMetalGraphics->tCurrentRenderEncoder endEncoding];
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLDevice> tDevice = ptMetalDevice->tDevice;

    uint32_t uCurrentVertexBuffer = UINT32_MAX;

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];

        for(uint32_t j = 0; j < ptArea->uDrawCount; j++)
        {
            plDraw* ptDraw = &atDraws[ptArea->uDrawOffset];

            if(uCurrentVertexBuffer != ptDraw->ptMesh->uVertexBuffer)
            {
                uCurrentVertexBuffer = ptDraw->ptMesh->uVertexBuffer;
                [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:(__bridge id)ptGraphics->tDevice.sbtBuffers[uCurrentVertexBuffer].pBuffer offset:ptDraw->ptMesh->uVertexOffset atIndex:0];  
            }

            [ptMetalGraphics->tCurrentRenderEncoder setDepthStencilState:ptMetalGraphics->tDepthStencilState];
            [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:(__bridge id)ptGraphics->tDevice.sbtBuffers[ptDraw->ptMesh->uVertexBuffer].pBuffer offset:0 atIndex:0];
            [ptMetalGraphics->tCurrentRenderEncoder setRenderPipelineState:ptMetalGraphics->tRenderPipelineState];
            [ptMetalGraphics->tCurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:ptDraw->ptMesh->uIndexCount indexType:MTLIndexTypeUInt32 indexBuffer:((__bridge id)ptGraphics->tDevice.sbtBuffers[ptDraw->ptMesh->uIndexBuffer].pBuffer) indexBufferOffset:0];

        }
        
    }
}

static void
pl_cleanup(plGraphics* ptGraphics)
{
    pl_cleanup_metal();

    for(uint32_t i = 0u; i < pl_sb_size(ptGraphics->sbt3DDrawlists); i++)
    {
        plDrawList3D* drawlist = ptGraphics->sbt3DDrawlists[i];
        pl_sb_free(drawlist->sbtSolidIndexBuffer);
        pl_sb_free(drawlist->sbtSolidVertexBuffer);
        pl_sb_free(drawlist->sbtLineVertexBuffer);
        pl_sb_free(drawlist->sbtLineIndexBuffer);  
    }
    pl_sb_free(ptGraphics->sbt3DDrawlists);
}

static void
pl_draw_lists(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLDevice> tDevice = ptMetalDevice->tDevice;

    plIO* ptIOCtx = pl_get_io();
    for(uint32_t i = 0; i < uListCount; i++)
    {
        pl_submit_metal_drawlist(&atLists[i], ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], ptMetalGraphics->tCurrentRenderEncoder, ptMetalGraphics->drawableRenderDescriptor);
    }
}

static void
pl__submit_3d_drawlist(plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags)
{
    plGraphics* ptGfx = ptDrawlist->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGfx->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptGfx->tDevice._pInternalData;

    plMetalPipelineEntry* ptPipelineEntry = pl__get_3d_pipelines(ptGfx, tFlags);

    const float fAspectRatio = fWidth / fHeight;

    const uint32_t uTotalIdxBufSzNeeded = sizeof(uint32_t) * (pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) + pl_sb_size(ptDrawlist->sbtLineIndexBuffer));
    const uint32_t uSolidVtxBufSzNeeded = sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);
    const uint32_t uLineVtxBufSzNeeded = sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer);

    plMetalBuffer* tIndexBuffer = pl__dequeue_reusable_buffer(ptGfx, uTotalIdxBufSzNeeded);
    plMetalBuffer* tVertexBuffer = pl__dequeue_reusable_buffer(ptGfx, uLineVtxBufSzNeeded + uSolidVtxBufSzNeeded);
    uint32_t uVertexOffset = 0;
    uint32_t uIndexOffset = 0;

    [ptMetalGraphics->tCurrentRenderEncoder setDepthStencilState:ptPipelineEntry->tDepthStencilState];
    [ptMetalGraphics->tCurrentRenderEncoder setCullMode:(tFlags & PL_PIPELINE_FLAG_FRONT_FACE_CW)];
    int iCullMode = MTLCullModeNone;
    if(tFlags & PL_PIPELINE_FLAG_CULL_FRONT) iCullMode = MTLCullModeFront;
    if(tFlags & PL_PIPELINE_FLAG_CULL_BACK) iCullMode |= MTLCullModeBack;
    [ptMetalGraphics->tCurrentRenderEncoder setCullMode:iCullMode];
    [ptMetalGraphics->tCurrentRenderEncoder setFrontFacingWinding:(tFlags & PL_PIPELINE_FLAG_FRONT_FACE_CW) ? MTLWindingClockwise : MTLWindingCounterClockwise];

    if(pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) > 0)
    {
        memcpy(tVertexBuffer.buffer.contents, ptDrawlist->sbtSolidVertexBuffer, uSolidVtxBufSzNeeded);
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);
        memcpy(tIndexBuffer.buffer.contents, ptDrawlist->sbtSolidIndexBuffer, uIdxBufSzNeeded);

        [ptMetalGraphics->tCurrentRenderEncoder setVertexBytes:ptMVP length:sizeof(plMat4) atIndex:1 ];
        
        [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:tVertexBuffer.buffer offset:uVertexOffset atIndex:0];
        [ptMetalGraphics->tCurrentRenderEncoder setRenderPipelineState:ptPipelineEntry->tSolidRenderPipelineState];
        [ptMetalGraphics->tCurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) indexType:MTLIndexTypeUInt32 indexBuffer:tIndexBuffer.buffer indexBufferOffset:uIndexOffset];

        uVertexOffset = uSolidVtxBufSzNeeded;
        uIndexOffset = uIdxBufSzNeeded;
    }

    if(pl_sb_size(ptDrawlist->sbtLineVertexBuffer) > 0)
    {
        memcpy(&((char*)tVertexBuffer.buffer.contents)[uVertexOffset], ptDrawlist->sbtLineVertexBuffer, uLineVtxBufSzNeeded);
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer);
        memcpy(&((char*)tIndexBuffer.buffer.contents)[uIndexOffset], ptDrawlist->sbtLineIndexBuffer, uIdxBufSzNeeded);

        struct UniformData {
            plMat4 tMvp;
            float  fAspect;
            float  padding[3];
        };

        struct UniformData b = {
            *ptMVP,
            fAspectRatio
        };

        [ptMetalGraphics->tCurrentRenderEncoder setVertexBytes:&b length:sizeof(struct UniformData) atIndex:1 ];
        [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:tVertexBuffer.buffer offset:uVertexOffset atIndex:0];
        [ptMetalGraphics->tCurrentRenderEncoder setRenderPipelineState:ptPipelineEntry->tLineRenderPipelineState];
        [ptMetalGraphics->tCurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:pl_sb_size(ptDrawlist->sbtLineIndexBuffer) indexType:MTLIndexTypeUInt32 indexBuffer:tIndexBuffer.buffer indexBufferOffset:uIndexOffset];
    }

    [ptMetalGraphics->tCurrentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> tCmdBuffer)
    {
        dispatch_async(dispatch_get_main_queue(), ^{

            @synchronized(ptMetalGraphics->bufferCache)
            {
                [ptMetalGraphics->bufferCache addObject:tVertexBuffer];
                [ptMetalGraphics->bufferCache addObject:tIndexBuffer];
            }
        });
    }];
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static const plGraphicsI*
pl_load_graphics_api(void)
{
    static const plGraphicsI tApi = {
        .initialize             = pl_initialize_graphics,
        .resize                 = pl_resize,
        .begin_frame            = pl_begin_frame,
        .end_frame              = pl_end_gfx_frame,
        .begin_recording        = pl_begin_recording,
        .end_recording          = pl_end_recording,
        .draw_areas             = pl_draw_areas,
        .draw_lists             = pl_draw_lists,
        .cleanup                = pl_cleanup,
        .create_font_atlas      = pl_create_metal_font_texture,
        .destroy_font_atlas     = pl_cleanup_metal_font_texture,
        .add_3d_triangle_filled = pl__add_3d_triangle_filled,
        .add_3d_line            = pl__add_3d_line,
        .add_3d_point           = pl__add_3d_point,
        .add_3d_transform       = pl__add_3d_transform,
        .add_3d_frustum         = pl__add_3d_frustum,
        .add_3d_centered_box    = pl__add_3d_centered_box,
        .add_3d_bezier_quad     = pl__add_3d_bezier_quad,
        .add_3d_bezier_cubic    = pl__add_3d_bezier_cubic,
        .register_3d_drawlist   = pl__register_3d_drawlist,
        .submit_3d_drawlist     = pl__submit_3d_drawlist
    };
    return &tApi;
}

static const plDeviceI*
pl_load_device_api(void)
{
    static const plDeviceI tApi = {
        .create_index_buffer = pl_create_index_buffer,
        .create_vertex_buffer = pl_create_vertex_buffer
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_context(ptDataRegistry->get_data("ui"));
    gptFile = ptApiRegistry->first(PL_API_FILE);
    if(bReload)
    {
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_GRAPHICS), pl_load_graphics_api());
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DEVICE), pl_load_device_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_GRAPHICS, pl_load_graphics_api());
        ptApiRegistry->add(PL_API_DEVICE, pl_load_device_api());
    }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryApiI* ptApiRegistry)
{

}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_ui_metal.m"