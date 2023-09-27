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
// [SECTION] internal api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_os.h"
#include "pl_profile.h"
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

const plFileApiI* gptFile = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal structs & types
//-----------------------------------------------------------------------------

@interface plTrackedMetalBuffer : NSObject
@property (nonatomic, strong) id<MTLBuffer> buffer;
@property (nonatomic, assign) double        lastReuseTime;
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer;
@end

@implementation plTrackedMetalBuffer
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

typedef struct _plMetalBuffer
{
    id<MTLBuffer> tBuffer;
} plMetalBuffer;

typedef struct _plMetalTexture
{
    id<MTLTexture> tTexture;
} plMetalTexture;

typedef struct _plMetalSampler
{
    id<MTLSamplerState> tSampler;
} plMetalSampler;

typedef struct _plMetalBindGroup
{
    id<MTLBuffer> tShaderArgumentBuffer;
} plMetalBindGroup;

typedef struct _plMetalShader
{
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;
} plMetalShader;

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

    uint64_t                 uNextFenceValue;
    id<MTLSharedEvent>       tStagingEvent;
    MTLSharedEventListener*  ptSharedEventListener;
    id<MTLBuffer>            tDynamicBuffer;
    id<MTLBuffer>            tStagingBuffer;
    id<MTLHeap>              tMemoryHeap;
    uint32_t                 uMemoryHeapOffset;
    
    plMetalTexture*   sbtTextures;
    plMetalSampler*   sbtSamplers;
    plMetalBindGroup* sbtBindGroups;
    plMetalBuffer*    sbtBuffers;
    plMetalShader*    sbtShaders;

    // drawing
    plMetalPipelineEntry*           sbtPipelineEntries;
    id<MTLFunction>                 tSolidVertexFunction;
    id<MTLFunction>                 tLineVertexFunction;
    id<MTLFunction>                 tFragmentFunction;
    NSMutableArray<plTrackedMetalBuffer*>* bufferCache;
    double                          lastBufferCachePurge;

    // per frame
    id<CAMetalDrawable>         tCurrentDrawable;
    id<MTLCommandBuffer>        tCurrentCommandBuffer;
    id<MTLRenderCommandEncoder> tCurrentRenderEncoder;

    // frames in flight
    dispatch_semaphore_t tFrameBoundarySemaphore;

    uint32_t uDynamicBufferHandle[2];
    uint32_t uDynamicByteOffset;

} plGraphicsMetal;

typedef struct _plDeviceMetal
{
    id<MTLDevice> tDevice;
    plGraphics* ptGraphics;
} plDeviceMetal;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plTrackedMetalBuffer* pl__dequeue_reusable_buffer(plGraphics* ptGraphics, NSUInteger length);
static plMetalPipelineEntry* pl__get_3d_pipelines(plGraphics* ptGraphics, pl3DDrawFlags tFlags);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plBuffer
pl_create_buffer(plDevice* ptDevice, const plBufferDescription* ptDesc)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptMetalDevice->ptGraphics->_pInternalData;

    plBuffer tBuffer = {
        .tDescription = *ptDesc,
        .uHandle = pl_sb_size(ptMetalGraphics->sbtBuffers)
    };

    if(ptDesc->tMemory == PL_MEMORY_GPU_CPU)
    {
        plMetalBuffer tMetalBuffer = {
            .tBuffer = [ptMetalDevice->tDevice newBufferWithLength:ptDesc->uByteSize options:MTLResourceStorageModeShared]
        };
        tMetalBuffer.tBuffer.label = [NSString stringWithUTF8String:ptDesc->pcDebugName];
        memset(tMetalBuffer.tBuffer.contents, 0, ptDesc->uByteSize);
        pl_sb_push(ptMetalGraphics->sbtBuffers, tMetalBuffer);

        if(ptDesc->puInitialData)
            memcpy(tMetalBuffer.tBuffer.contents, ptDesc->puInitialData, ptDesc->uInitialDataByteSize);

        tBuffer.pcData = tMetalBuffer.tBuffer.contents;
    }
    else if(ptDesc->tMemory == PL_MEMORY_GPU)
    {
        // copy from cpu to gpu once staging buffer is free
        [ptMetalGraphics->tStagingEvent notifyListener:ptMetalGraphics->ptSharedEventListener
                            atValue:ptMetalGraphics->uNextFenceValue++
                            block:^(id<MTLSharedEvent> sharedEvent, uint64_t value) {
            if(ptDesc->puInitialData)
                memcpy(ptMetalGraphics->tStagingBuffer.contents, ptDesc->puInitialData, ptDesc->uInitialDataByteSize);
            sharedEvent.signaledValue = ptMetalGraphics->uNextFenceValue;
        }];

        // wait for cpu to gpu copying to take place before continuing
        while(true)
        {
            if(ptMetalGraphics->tStagingEvent.signaledValue == ptMetalGraphics->uNextFenceValue)
                break;
        }
        
        id<MTLCommandBuffer> commandBuffer = [ptMetalGraphics->tCmdQueue commandBuffer];
        commandBuffer.label = @"Heap Transfer Blit Encoder";

        [commandBuffer encodeWaitForEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue++];

        id<MTLBlitCommandEncoder> blitEncoder = commandBuffer.blitCommandEncoder;
        blitEncoder.label = @"Heap Transfer Blit Encoder";

        MTLSizeAndAlign tSizeAndAlign = [ptMetalDevice->tDevice heapBufferSizeAndAlignWithLength:ptDesc->uByteSize options:MTLResourceStorageModePrivate];

        plMetalBuffer tMetalBuffer = {
            .tBuffer = [ptMetalGraphics->tMemoryHeap newBufferWithLength:ptDesc->uByteSize options:MTLResourceStorageModePrivate offset:ptMetalGraphics->uMemoryHeapOffset]
        };
        tMetalBuffer.tBuffer.label = [NSString stringWithUTF8String:ptDesc->pcDebugName];

        [blitEncoder copyFromBuffer:ptMetalGraphics->tStagingBuffer sourceOffset:0 toBuffer:tMetalBuffer.tBuffer destinationOffset:0 size:ptDesc->uByteSize];

        [blitEncoder endEncoding];
        [commandBuffer encodeSignalEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue];
        [commandBuffer commit];

        pl_sb_push(ptMetalGraphics->sbtBuffers, tMetalBuffer);
        ptMetalGraphics->uMemoryHeapOffset += ptDesc->uByteSize;
        ptMetalGraphics->uMemoryHeapOffset = PL_ALIGN_UP(ptMetalGraphics->uMemoryHeapOffset, 256);
    }

    return tBuffer;
}

static plTexture
pl_create_texture(plDevice* ptDevice, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptMetalDevice->ptGraphics->_pInternalData;

    plTexture tTexture = {
        .tDesc = tDesc,
        .uHandle = pl_sb_size(ptMetalGraphics->sbtTextures)
    };

    MTLTextureDescriptor* ptTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    ptTextureDescriptor.pixelFormat = MTLPixelFormatRGBA32Float;
    ptTextureDescriptor.width = tDesc.tDimensions.x;
    ptTextureDescriptor.height = tDesc.tDimensions.y;
    ptTextureDescriptor.mipmapLevelCount = tDesc.uMips;

    plMetalTexture tMetalTexture = {
        .tTexture = [ptMetalDevice->tDevice newTextureWithDescriptor:ptTextureDescriptor]
    };

    MTLRegion tRegion = {
        {0},
        {tDesc.tDimensions.x, tDesc.tDimensions.y, 1}
    };
    NSUInteger uBytesPerRow = tDesc.tDimensions.x * 4 * sizeof(float);
    [tMetalTexture.tTexture replaceRegion:tRegion
        mipmapLevel: 0
        withBytes: pData
        bytesPerRow:uBytesPerRow];

    pl_sb_push(ptMetalGraphics->sbtTextures, tMetalTexture);
    return tTexture;
}


static plTextureView
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, uint32_t uTextureHandle, const char* pcName)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptMetalDevice->ptGraphics->_pInternalData;

    plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTextures[uTextureHandle];

    plTextureView tTextureView = {
        .tSampler         = *ptSampler,
        .tTextureViewDesc = *ptViewDesc,
        .uTextureHandle   = uTextureHandle,
        ._uSamplerHandle  = pl_sb_size(ptMetalGraphics->sbtSamplers)
    };

    MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
    samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
    samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
    samplerDesc.normalizedCoordinates = YES;
    samplerDesc.supportArgumentBuffers = YES;

    plMetalSampler tMetalSampler = {
        .tSampler = [ptMetalDevice->tDevice newSamplerStateWithDescriptor:samplerDesc]
    };

    pl_sb_push(ptMetalGraphics->sbtSamplers, tMetalSampler);
    return tTextureView;
}

static plBindGroup
pl_create_bind_group(plDevice* ptDevice, plBindGroupLayout* ptLayout)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptMetalDevice->ptGraphics->_pInternalData;

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout,
        .uHandle = pl_sb_size(ptMetalGraphics->sbtBindGroups)
    };

    NSUInteger argumentBufferLength = sizeof(MTLResourceID) * ptLayout->uTextureCount * 2 + sizeof(void*) * ptLayout->uBufferCount;

    plMetalBindGroup tMetalBindGroup = {
        .tShaderArgumentBuffer = [ptMetalDevice->tDevice  newBufferWithLength:argumentBufferLength options:0]
    };

    pl_sb_push(ptMetalGraphics->sbtBindGroups, tMetalBindGroup);
    return tBindGroup;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroup* ptGroup, uint32_t uBufferCount, plBuffer* atBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, plTextureView* atTextureViews)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptMetalDevice->ptGraphics->_pInternalData;

    plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroups[ptGroup->uHandle];

    // start of buffers
    float** ptBufferResources = (float**)ptMetalBindGroup->tShaderArgumentBuffer.contents;
    for(uint32_t i = 0; i < uBufferCount; i++)
    {
        plMetalBuffer* ptMetalBuffer = &ptMetalGraphics->sbtBuffers[atBuffers[i].uHandle];
        ptBufferResources[i] = (float*)ptMetalBuffer->tBuffer.gpuAddress;
        ptGroup->tLayout.aBuffers[i].tBuffer = atBuffers[i];
    }

    // start of textures
    char* pcStartOfBuffers = (char*)ptMetalBindGroup->tShaderArgumentBuffer.contents;

    MTLResourceID* ptResources = (MTLResourceID*)(&pcStartOfBuffers[sizeof(void*) * uBufferCount]);
    for(uint32_t i = 0; i < uTextureViewCount; i++)
    {
        plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTextures[atTextureViews[i].uTextureHandle];
        plMetalSampler* ptMetalSampler = &ptMetalGraphics->sbtSamplers[atTextureViews[i]._uSamplerHandle];
        ptResources[i * 2] = ptMetalTexture->tTexture.gpuResourceID;
        ptResources[i * 2 + 1] = ptMetalSampler->tSampler.gpuResourceID;
        ptGroup->tLayout.aTextures[i].tTextureView = atTextureViews[i];
    }
}

static plDynamicBinding
pl_allocate_dynamic_data(plDevice* ptDevice, size_t szSize)
{
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptMetalDevice->ptGraphics->_pInternalData;

    plMetalBuffer* ptMetalBuffer =  &ptMetalGraphics->sbtBuffers[ptMetalGraphics->uDynamicBufferHandle[ptMetalDevice->ptGraphics->uCurrentFrameIndex]];

    plDynamicBinding tDynamicBinding = {
        .uBufferHandle = ptMetalGraphics->uDynamicBufferHandle[ptMetalDevice->ptGraphics->uCurrentFrameIndex],
        .uByteOffset   = ptMetalGraphics->uDynamicByteOffset,
        .pcData        = &ptMetalBuffer->tBuffer.contents[ptMetalGraphics->uDynamicByteOffset]
    };
    ptMetalGraphics->uDynamicByteOffset = pl_align_up((size_t)ptMetalGraphics->uDynamicByteOffset + szSize, 256);
    return tDynamicBinding;
}

static plShader
pl_create_shader(plGraphics* ptGraphics, plShaderDescription* ptDescription)
{
    
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    plShader tShader = {
        .tDescription = *ptDescription,
        .uHandle      = pl_sb_size(ptMetalGraphics->sbtShaders)
    };

    // read in shader source code
    unsigned uShaderFileSize = 0;
    gptFile->read(ptDescription->pcVertexShader, &uShaderFileSize, NULL, "rb");
    char* pcFileData = PL_ALLOC(uShaderFileSize + 1);
    gptFile->read(ptDescription->pcVertexShader, &uShaderFileSize, pcFileData, "rb");

    // prepare preprocessor defines
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];

    int iDataStride = 0;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)
        iDataStride += 4;

    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0)
        iDataStride += 4;

    int iTextureCount = 0;
    if(ptDescription->tGraphicsState.ulShaderTextureFlags & PL_SHADER_TEXTURE_FLAG_BINDING_0)
        iTextureCount++;

    ptCompileOptions.preprocessorMacros = @{
        @"PL_TEXTURE_COUNT" : [NSNumber numberWithInt:iTextureCount],
        @"PL_BUFFER_COUNT" : [NSNumber numberWithInt:ptDescription->atBindGroupLayouts[1].uBufferCount + 1],
        @"PL_DATA_STRIDE" : [NSNumber numberWithInt:iDataStride],
        @"PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)],
        @"PL_MESH_FORMAT_FLAG_HAS_COLOR_0" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0)]
    };

    // compile shader source
    NSError* error = nil;
    NSString* shaderSource = [NSString stringWithUTF8String:pcFileData];
    id<MTLLibrary> library = [ptMetalDevice->tDevice  newLibraryWithSource:shaderSource options:ptCompileOptions error:&error];
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

    // vertex layout
    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3; // position
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stride = sizeof(float) * 3;

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.rasterSampleCount = 1;

    // renderpass stuff

    MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual;
    depthDescriptor.depthWriteEnabled = YES;

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

    const plMetalShader tMetalShader = {
        .tDepthStencilState = [ptMetalDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor],
        .tRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error]
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    pl_sb_push(ptMetalGraphics->sbtShaders, tMetalShader);

    PL_FREE(pcFileData);
    return tShader;
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
    ptMetalDevice->ptGraphics = ptGraphics;
    ptMetalDevice->tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    ptGraphics->uFramesInFlight = 2;

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

    // line rendering
    {
        NSError* error = nil;

        // read in shader source code
        unsigned uShaderFileSize0 = 0;
        gptFile->read("../shaders/metal/draw_3d_line.metal", &uShaderFileSize0, NULL, "r");
        char* pcFileData0 = PL_ALLOC(uShaderFileSize0 + 1);
        gptFile->read("../shaders/metal/draw_3d_line.metal", &uShaderFileSize0, pcFileData0, "r");
        NSString* lineShaderSource = [NSString stringWithUTF8String:pcFileData0];


        id<MTLLibrary> library = [ptMetalDevice->tDevice newLibraryWithSource:lineShaderSource options:nil error:&error];
        if (library == nil)
        {
            NSLog(@"Error: failed to create Metal library: %@", error);
        }

        ptMetalGraphics->tLineVertexFunction = [library newFunctionWithName:@"vertex_main"];
        ptMetalGraphics->tFragmentFunction = [library newFunctionWithName:@"fragment_main"];

        unsigned uShaderFileSize1 = 0;
        gptFile->read("../shaders/metal/draw_3d.metal", &uShaderFileSize1, NULL, "r");
        char* pcFileData1 = PL_ALLOC(uShaderFileSize1 + 1);
        gptFile->read("../shaders/metal/draw_3d.metal", &uShaderFileSize1, pcFileData1, "r");

        NSString* solidShaderSource = [NSString stringWithUTF8String:pcFileData1];
        id<MTLLibrary> library1 = [ptMetalDevice->tDevice newLibraryWithSource:solidShaderSource options:nil error:&error];
        if (library1 == nil)
        {
            NSLog(@"Error: failed to create Metal library: %@", error);
        }

        ptMetalGraphics->tSolidVertexFunction = [library1 newFunctionWithName:@"vertex_main"];

        PL_FREE(pcFileData0);
        PL_FREE(pcFileData1);
    }

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = MTLStorageModePrivate;
    ptHeapDescriptor.size = 0;
    ptHeapDescriptor.type = MTLHeapTypePlacement;

    MTLSizeAndAlign sizeAndAlign = [ptMetalDevice->tDevice heapBufferSizeAndAlignWithLength:134217728
        options:MTLResourceStorageModePrivate];

    sizeAndAlign.size += (sizeAndAlign.size & (sizeAndAlign.align - 1)) + sizeAndAlign.align;

    ptHeapDescriptor.size += sizeAndAlign.size;

    ptMetalGraphics->tMemoryHeap = [ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    ptMetalGraphics->tDynamicBuffer = [ptMetalDevice->tDevice newBufferWithLength:134217728 options:MTLResourceStorageModeShared];
    ptMetalGraphics->tStagingBuffer = [ptMetalDevice->tDevice newBufferWithLength:134217728 options:MTLResourceStorageModeShared];
    ptMetalGraphics->tStagingEvent = [ptMetalDevice->tDevice newSharedEvent];
    dispatch_queue_t tQueue = dispatch_queue_create("com.example.apple-samplecode.MyQueue", NULL);
    ptMetalGraphics->ptSharedEventListener = [[MTLSharedEventListener alloc] initWithDispatchQueue:tQueue];

    ptMetalGraphics->tFrameBoundarySemaphore = dispatch_semaphore_create(ptGraphics->uFramesInFlight);

    // dynamic buffer stuff
    plMetalBuffer tMetalBuffer0 = {
        .tBuffer = [ptMetalDevice->tDevice newBufferWithLength:134217728 options:MTLResourceStorageModeShared]
    };
    tMetalBuffer0.tBuffer.label = @"Dynamic Buffer 0";

    plMetalBuffer tMetalBuffer1 = {
        .tBuffer = [ptMetalDevice->tDevice newBufferWithLength:134217728 options:MTLResourceStorageModeShared]
    };
    tMetalBuffer0.tBuffer.label = @"Dynamic Buffer 1";

    memset(tMetalBuffer0.tBuffer.contents, 0, 134217728);
    memset(tMetalBuffer1.tBuffer.contents, 0, 134217728);

    ptMetalGraphics->uDynamicBufferHandle[0] = pl_sb_size(ptMetalGraphics->sbtBuffers);
    ptMetalGraphics->uDynamicBufferHandle[1] = pl_sb_size(ptMetalGraphics->sbtBuffers) + 1;

    pl_sb_push(ptMetalGraphics->sbtBuffers, tMetalBuffer0);
    pl_sb_push(ptMetalGraphics->sbtBuffers, tMetalBuffer1);
}

static void
pl_resize(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
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
    pl_end_profile_sample();
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    ptMetalGraphics->uDynamicByteOffset = 0;

    // Wait until the inflight command buffer has completed its work
    dispatch_semaphore_wait(ptMetalGraphics->tFrameBoundarySemaphore, DISPATCH_TIME_FOREVER);
    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;

    plIO* ptIOCtx = pl_get_io();
    ptMetalGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;
    
    ptMetalGraphics->uCurrentFrame++;

        // get next drawable
    ptMetalGraphics->tCurrentDrawable = [ptMetalGraphics->pMetalLayer nextDrawable];

    if(!ptMetalGraphics->tCurrentDrawable)
    {
        pl_end_profile_sample();
        return false;
    }

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

    pl_end_profile_sample();
    return true;
}

static void
pl_end_gfx_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;

    [ptMetalGraphics->tCurrentCommandBuffer presentDrawable:ptMetalGraphics->tCurrentDrawable];

    dispatch_semaphore_t semaphore = ptMetalGraphics->tFrameBoundarySemaphore;
    [ptMetalGraphics->tCurrentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
        // GPU work is complete
        // Signal the semaphore to start the CPU work
        dispatch_semaphore_signal(semaphore);
    }];

    [ptMetalGraphics->tCurrentCommandBuffer commit];

    
    pl_end_profile_sample();
}

static void
pl_begin_recording(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    ptMetalGraphics->tCurrentRenderEncoder = [ptMetalGraphics->tCurrentCommandBuffer renderCommandEncoderWithDescriptor:ptMetalGraphics->drawableRenderDescriptor];
    pl_end_profile_sample();
}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    [ptMetalGraphics->tCurrentRenderEncoder endEncoding];
    pl_end_profile_sample();
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLDevice> tDevice = ptMetalDevice->tDevice;

    [ptMetalGraphics->tCurrentRenderEncoder useHeap:ptMetalGraphics->tMemoryHeap stages:MTLRenderStageVertex];

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];

        for(uint32_t j = 0; j < ptArea->uDrawCount; j++)
        {
            plDraw* ptDraw = &atDraws[ptArea->uDrawOffset + j];
            plMetalShader* ptMetalShader = &ptMetalGraphics->sbtShaders[ptDraw->uShaderVariant];

            plMetalBindGroup* ptMetalBindGroup0 = &ptMetalGraphics->sbtBindGroups[ptDraw->aptBindGroups[0]->uHandle];

            // pipeline state
            [ptMetalGraphics->tCurrentRenderEncoder setCullMode:MTLCullModeBack];
            [ptMetalGraphics->tCurrentRenderEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
            [ptMetalGraphics->tCurrentRenderEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
            
            for(uint32_t k = 0; k < ptDraw->aptBindGroups[1]->tLayout.uTextureCount; k++)
            {
                [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtTextures[ptDraw->aptBindGroups[1]->tLayout.aTextures[k].tTextureView.uTextureHandle].tTexture
                    usage:MTLResourceUsageRead
                    stages:MTLRenderStageFragment];  
            }

            // make resources available to gpu
            for(uint32_t k = 0; k < ptDraw->aptBindGroups[2]->tLayout.uBufferCount; k++)
            {
                [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtBuffers[ptDraw->aptBindGroups[2]->tLayout.aBuffers[k].tBuffer.uHandle].tBuffer
                    usage:MTLResourceUsageRead
                    stages:MTLRenderStageVertex];
            }
            
            [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtBuffers[ptDraw->aptBindGroups[0]->tLayout.aBuffers[0].tBuffer.uHandle].tBuffer
                usage:MTLResourceUsageRead
                stages:MTLRenderStageVertex];

            // bind group 0
            [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalBindGroup0->tShaderArgumentBuffer
                offset:0
                atIndex:1];

            // bind group 0
            [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptMetalBindGroup0->tShaderArgumentBuffer
                offset:0
                atIndex:1];

            // bind group 1
            [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptMetalGraphics->sbtBindGroups[ptDraw->aptBindGroups[1]->uHandle].tShaderArgumentBuffer
                offset:0
                atIndex:2];

            // bind group 1
            [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalGraphics->sbtBindGroups[ptDraw->aptBindGroups[1]->uHandle].tShaderArgumentBuffer
                offset:0
                atIndex:2];

            // bind group 2
            [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptMetalGraphics->sbtBindGroups[ptDraw->aptBindGroups[2]->uHandle].tShaderArgumentBuffer
                offset:0
                atIndex:3];

            // bind group 2
            [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalGraphics->sbtBindGroups[ptDraw->aptBindGroups[2]->uHandle].tShaderArgumentBuffer
                offset:0
                atIndex:3];

            // bind group 3
            [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalGraphics->sbtBuffers[ptDraw->uDynamicBuffer].tBuffer
                offset:0
                atIndex:4];

            // bind group 3
            [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptMetalGraphics->sbtBuffers[ptDraw->uDynamicBuffer].tBuffer
                offset:0
                atIndex:4];

            [ptMetalGraphics->tCurrentRenderEncoder setVertexBufferOffset:ptDraw->auDynamicBufferOffset[0] atIndex:4];
            [ptMetalGraphics->tCurrentRenderEncoder setFragmentBufferOffset:ptDraw->auDynamicBufferOffset[0] atIndex:4];
            
            // set vertex buffer
            [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalGraphics->sbtBuffers[ptDraw->uVertexBuffer].tBuffer
                offset:0
                atIndex:0];

            // set index buffer & draw
            [ptMetalGraphics->tCurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
                indexCount:ptDraw->uIndexCount
                indexType:MTLIndexTypeUInt32
                indexBuffer:ptMetalGraphics->sbtBuffers[ptDraw->uIndexBuffer].tBuffer
                indexBufferOffset:0
                instanceCount:1
                baseVertex:ptDraw->uVertexOffset
                baseInstance:0
                ];

        }
        
    }
    pl_end_profile_sample();
}

static void
pl_cleanup(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;

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

    pl_sb_free(ptMetalGraphics->sbtTextures);
    pl_sb_free(ptMetalGraphics->sbtSamplers);
    pl_sb_free(ptMetalGraphics->sbtBindGroups);
    pl_sb_free(ptMetalGraphics->sbtBuffers);
    pl_sb_free(ptMetalGraphics->sbtShaders);

    pl_sb_free(ptMetalGraphics->sbtPipelineEntries);
    PL_FREE(ptGraphics->_pInternalData);
    PL_FREE(ptGraphics->tDevice._pInternalData);
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

    plTrackedMetalBuffer* tIndexBuffer = pl__dequeue_reusable_buffer(ptGfx, uTotalIdxBufSzNeeded);
    plTrackedMetalBuffer* tVertexBuffer = pl__dequeue_reusable_buffer(ptGfx, uLineVtxBufSzNeeded + uSolidVtxBufSzNeeded);
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
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plTrackedMetalBuffer*
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
            for (plTrackedMetalBuffer* candidate in ptMetalGraphics->bufferCache)
                if (candidate.lastReuseTime > ptMetalGraphics->lastBufferCachePurge)
                    [survivors addObject:candidate];
            ptMetalGraphics->bufferCache = [survivors mutableCopy];
            ptMetalGraphics->lastBufferCachePurge = now;
        }

        // see if we have a buffer we can reuse
        plTrackedMetalBuffer* bestCandidate = nil;
        for (plTrackedMetalBuffer* candidate in ptMetalGraphics->bufferCache)
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
    return [[plTrackedMetalBuffer alloc] initWithBuffer:backing];
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
        .create_shader          = pl_create_shader,
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
        .create_buffer         = pl_create_buffer,
        .create_texture_view   = pl_create_texture_view,
        .create_texture        = pl_create_texture,
        .create_bind_group     = pl_create_bind_group,
        .update_bind_group     = pl_update_bind_group,
        .allocate_dynamic_data = pl_allocate_dynamic_data
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_context(ptDataRegistry->get_data("ui"));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
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