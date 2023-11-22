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

typedef struct _plMetalDynamicBuffer
{
    uint32_t                 uByteOffset;
    uint32_t                 uHandle;
    plDeviceMemoryAllocation tMemory;
    id<MTLBuffer>            tBuffer;
} plMetalDynamicBuffer;

typedef struct _plMetalFrameBuffer
{
    int a;
} plMetalFrameBuffer;

typedef struct _plMetalSwapchain
{
    int unused;
} plMetalSwapchain;

typedef struct _plMetalRenderPass
{
    MTLRenderPassDescriptor* ptRenderPassDescriptor;
} plMetalRenderPass;

typedef struct _plFrameContext
{
    dispatch_semaphore_t tFrameBoundarySemaphore;

    // dynamic buffer stuff
    uint32_t              uCurrentBufferIndex;
    plMetalDynamicBuffer* sbtDynamicBuffers;
} plFrameContext;

typedef struct _plMetalBuffer
{
    id<MTLBuffer> tBuffer;
    id<MTLHeap>   tHeap;
} plMetalBuffer;

typedef struct _plMetalTexture
{
    id<MTLTexture> tTexture;
    id<MTLHeap>    tHeap;
} plMetalTexture;

typedef struct _plMetalSampler
{
    id<MTLSamplerState> tSampler;
} plMetalSampler;

typedef struct _plMetalBindGroup
{
    id<MTLBuffer> tShaderArgumentBuffer;
    plBindGroupLayout tLayout;
} plMetalBindGroup;

typedef struct _plMetalShader
{
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;
    MTLCullMode                tCullMode;
} plMetalShader;

typedef struct _plMetalPipelineEntry
{
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tSolidRenderPipelineState;
    id<MTLRenderPipelineState> tLineRenderPipelineState;
    pl3DDrawFlags              tFlags;
    uint32_t                   uSampleCount;
} plMetalPipelineEntry;

typedef struct _plGraphicsMetal
{
    id<MTLCommandQueue> tCmdQueue;
    CAMetalLayer*       pMetalLayer;

    uint64_t                 uNextFenceValue;
    id<MTLSharedEvent>       tStagingEvent;
    MTLSharedEventListener*  ptSharedEventListener;
    id<MTLBuffer>            tStagingBuffer;
    plDeviceMemoryAllocation tStagingMemory;
    
    plFrameContext*    sbFrames;
    plMetalTexture*    sbtTexturesHot;
    plMetalSampler*    sbtSamplersHot;
    plMetalBindGroup*  sbtBindGroupsHot;
    plMetalBuffer*     sbtBuffersHot;
    plMetalShader*     sbtShadersHot;
    plMetalRenderPass* sbtRenderPassesHot;
    
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
} plGraphicsMetal;

typedef struct _plDeviceMetal
{
    id<MTLDevice> tDevice;
} plDeviceMetal;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// conversion between pilotlight & vulkan types
static MTLSamplerMinMagFilter pl__metal_filter(plFilter tFilter);
static MTLSamplerAddressMode  pl__metal_wrap(plWrapMode tWrap);
static MTLCompareFunction     pl__metal_compare(plCompareMode tCompare);
static MTLPixelFormat         pl__metal_format(plFormat tFormat);
static MTLCullMode            pl__metal_cull(plCullMode tCullMode);
static MTLLoadAction          pl__metal_load_op   (plLoadOp tOp);
static MTLStoreAction         pl__metal_store_op  (plStoreOp tOp);

static plTrackedMetalBuffer* pl__dequeue_reusable_buffer(plGraphics* ptGraphics, NSUInteger length);
static plMetalPipelineEntry* pl__get_3d_pipelines(plGraphics* ptGraphics, pl3DDrawFlags tFlags, uint32_t uSampleCount, MTLRenderPassDescriptor* ptRenderPassDescriptor);

// device memory allocators specifics
static plDeviceMemoryAllocation pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_dedicated    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocation pl_allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);
static void                     pl_free_staging_uncached    (struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation);

static plDeviceMemoryAllocation pl_allocate_buddy(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName);

// device memory allocator general
static plDeviceAllocationBlock* pl_get_allocator_blocks(struct plDeviceMemoryAllocatorO* ptInst, uint32_t* puSizeOut);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plFrameContext*
pl__get_frame_resources(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    return &ptMetalGraphics->sbFrames[ptGraphics->uCurrentFrameIndex];
}

static void*
pl_get_ui_texture_handle(plGraphics* ptGraphics, plTextureViewHandle tHandle)
{
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plTextureView* ptView = pl__get_texture_view(&ptGraphics->tDevice, tHandle);
    return ptMetalGraphics->sbtTexturesHot[ptView->tTexture.uIndex].tTexture;
}

static plFrameBufferHandle
pl_create_frame_buffer(plDevice* ptDevice, const plFrameBufferDescription* ptDescription)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uBufferIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtFrameBufferFreeIndices) > 0)
        uBufferIndex = pl_sb_pop(ptGraphics->sbtFrameBufferFreeIndices);
    else
    {
        uBufferIndex = pl_sb_size(ptGraphics->sbtFrameBuffersCold);
        pl_sb_add(ptGraphics->sbtFrameBuffersCold);
        pl_sb_push(ptGraphics->sbtFrameBufferGenerations, UINT32_MAX);
        // pl_sb_add(ptVulkanGfx->sbtFrameBuffersHot);
    }

    plFrameBufferHandle tHandle = {
        .uGeneration = ptGraphics->sbtFrameBufferGenerations[uBufferIndex],
        .uIndex = uBufferIndex
    };

    plFrameBuffer tFrameBuffer = {
        .tDescription = *ptDescription
    };

    ptGraphics->sbtFrameBuffersCold[uBufferIndex] = tFrameBuffer;
    ptGraphics->sbtFrameBufferGenerations[uBufferIndex]++;
    return tHandle;
}

static plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDescription* ptDesc)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;

    plRenderPassLayoutHandle tHandle = {
        .uGeneration = 0,
        .uIndex = pl_sb_size(ptGraphics->sbtRenderPassLayoutsCold)
    };

    plRenderPassLayout tLayout = {
        .tDesc = *ptDesc
    };

    // pl_sb_push(ptVulkanGfx->sbtRenderPassLayoutsHot, tVulkanRenderPass);
    pl_sb_push(ptGraphics->sbtRenderPassLayoutsCold, tLayout);
    return tHandle;
}

static plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDescription* ptDesc)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plRenderPassHandle tHandle = {
        .uGeneration = 0,
        .uIndex = pl_sb_size(ptMetalGraphics->sbtRenderPassesHot)
    };

    plRenderPass tRenderPass = {
        .tDesc = *ptDesc,
        .tSampleCount = 1
    };

    const plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptDesc->tLayout.uIndex];


    plMetalRenderPass tMetalRenderPass = {0};

    // render pass descriptor
    tMetalRenderPass.ptRenderPassDescriptor = [MTLRenderPassDescriptor new];

    if(ptLayout->tDesc.tDepthTarget.tFormat != PL_FORMAT_UNKNOWN)
    {
        tMetalRenderPass.ptRenderPassDescriptor.depthAttachment.loadAction = pl__metal_load_op(ptDesc->tDepthTarget.tLoadOp);
        tMetalRenderPass.ptRenderPassDescriptor.depthAttachment.storeAction = pl__metal_store_op(ptDesc->tDepthTarget.tStoreOp);
        tMetalRenderPass.ptRenderPassDescriptor.depthAttachment.clearDepth = ptDesc->tDepthTarget.fClearZ;
    }

    for(uint32_t i = 0; i < 16; i++)
    {
        if(ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
        {
            break;
        }

        if(ptLayout->tDesc.atRenderTargets[i].tSampleCount != 1)
            tRenderPass.tSampleCount = ptLayout->tDesc.atRenderTargets[i].tSampleCount;

        tMetalRenderPass.ptRenderPassDescriptor.colorAttachments[i].loadAction = pl__metal_load_op(ptDesc->atRenderTargets[i].tLoadOp);
        tMetalRenderPass.ptRenderPassDescriptor.colorAttachments[i].storeAction = pl__metal_store_op(ptDesc->atRenderTargets[i].tStoreOp);
        tMetalRenderPass.ptRenderPassDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(
            ptDesc->atRenderTargets[i].tClearColor.r,
            ptDesc->atRenderTargets[i].tClearColor.g,
            ptDesc->atRenderTargets[i].tClearColor.b,
            ptDesc->atRenderTargets[i].tClearColor.a
            );
    }

    pl_sb_push(ptMetalGraphics->sbtRenderPassesHot, tMetalRenderPass);
    pl_sb_push(ptGraphics->sbtRenderPassesCold, tRenderPass);

    return tHandle;
}

static plBufferHandle
pl_create_buffer(plDevice* ptDevice, const plBufferDescription* ptDesc, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uBufferIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtBufferFreeIndices) > 0)
        uBufferIndex = pl_sb_pop(ptGraphics->sbtBufferFreeIndices);
    else
    {
        uBufferIndex = pl_sb_size(ptGraphics->sbtBuffersCold);
        pl_sb_add(ptGraphics->sbtBuffersCold);
        pl_sb_push(ptGraphics->sbtBufferGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtBuffersHot);
    }

    plBufferHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtBufferGenerations[uBufferIndex],
        .uIndex = uBufferIndex
    };

    plBuffer tBuffer = {
        .tDescription = *ptDesc
    };

    if(pcName)
    {
        pl_sprintf(tBuffer.tDescription.acDebugName, "%s", pcName);
    }

    if(ptDesc->tMemory == PL_MEMORY_GPU_CPU)
    {
        tBuffer.tMemoryAllocation = ptDevice->tStagingUnCachedAllocator.allocate(ptDevice->tStagingUnCachedAllocator.ptInst, 0, ptDesc->uByteSize, 0, pcName);

        plMetalBuffer tMetalBuffer = {
            .tBuffer = [(id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle newBufferWithLength:ptDesc->uByteSize options:MTLResourceStorageModeShared offset:0]
        };
        tMetalBuffer.tBuffer.label = [NSString stringWithUTF8String:ptDesc->acDebugName];
        memset(tMetalBuffer.tBuffer.contents, 0, ptDesc->uByteSize);
        
        if(ptDesc->puInitialData)
            memcpy(tMetalBuffer.tBuffer.contents, ptDesc->puInitialData, ptDesc->uInitialDataByteSize);

        tBuffer.tMemoryAllocation.pHostMapped = tMetalBuffer.tBuffer.contents;
        tBuffer.tMemoryAllocation.ulOffset = 0;
        tBuffer.tMemoryAllocation.ulSize = ptDesc->uByteSize;
        tMetalBuffer.tHeap = (id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle;
        ptMetalGraphics->sbtBuffersHot[uBufferIndex] = tMetalBuffer;
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

        plDeviceMemoryAllocatorI* ptAllocator = ptDesc->uByteSize > PL_DEVICE_ALLOCATION_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
        tBuffer.tMemoryAllocation = ptAllocator->allocate(ptAllocator->ptInst, MTLStorageModePrivate, ptDesc->uByteSize, 0, pcName);

        id<MTLCommandBuffer> commandBuffer = [ptMetalGraphics->tCmdQueue commandBufferWithUnretainedReferences];
        commandBuffer.label = @"Heap Transfer Blit Encoder";

        [commandBuffer encodeWaitForEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue++];

        id<MTLBlitCommandEncoder> blitEncoder = commandBuffer.blitCommandEncoder;
        blitEncoder.label = @"Heap Transfer Blit Encoder";

        MTLSizeAndAlign tSizeAndAlign = [ptMetalDevice->tDevice heapBufferSizeAndAlignWithLength:ptDesc->uByteSize options:MTLResourceStorageModePrivate];

        plMetalBuffer tMetalBuffer = {
            .tBuffer = [(id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle newBufferWithLength:ptDesc->uByteSize options:MTLResourceStorageModePrivate offset:tBuffer.tMemoryAllocation.ulOffset]
        };
        tMetalBuffer.tBuffer.label = [NSString stringWithUTF8String:ptDesc->acDebugName];

        [blitEncoder copyFromBuffer:ptMetalGraphics->tStagingBuffer sourceOffset:0 toBuffer:tMetalBuffer.tBuffer destinationOffset:0 size:ptDesc->uByteSize];

        [blitEncoder endEncoding];
        [commandBuffer encodeSignalEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue];
        [commandBuffer commit];

        tMetalBuffer.tHeap = (id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle;
        ptMetalGraphics->sbtBuffersHot[uBufferIndex] = tMetalBuffer;
    }
    else if(ptDesc->tMemory == PL_MEMORY_CPU)
    {
        tBuffer.tMemoryAllocation = ptDevice->tStagingUnCachedAllocator.allocate(ptDevice->tStagingUnCachedAllocator.ptInst, MTLStorageModePrivate, ptDesc->uByteSize, 0, pcName);

        plMetalBuffer tMetalBuffer = {
            .tBuffer = [(id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle newBufferWithLength:ptDesc->uByteSize options:MTLResourceStorageModeShared offset:0]
        };
        tMetalBuffer.tBuffer.label = [NSString stringWithUTF8String:ptDesc->acDebugName];
        memset(tMetalBuffer.tBuffer.contents, 0, ptDesc->uByteSize);

        if(ptDesc->puInitialData)
            memcpy(tMetalBuffer.tBuffer.contents, ptDesc->puInitialData, ptDesc->uInitialDataByteSize);

        tBuffer.tMemoryAllocation.pHostMapped = tMetalBuffer.tBuffer.contents;
        tBuffer.tMemoryAllocation.ulOffset = 0;
        tBuffer.tMemoryAllocation.ulSize = ptDesc->uByteSize;
        tMetalBuffer.tHeap = (id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle;
        ptMetalGraphics->sbtBuffersHot[uBufferIndex] = tMetalBuffer;
    }

    ptGraphics->sbtBuffersCold[uBufferIndex] = tBuffer;
    return tHandle;
}

static void
pl_update_texture(plDevice* ptDevice, plTextureHandle tHandle, size_t szSize, const void* pData)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    // copy from cpu to gpu once staging buffer is free
    [ptMetalGraphics->tStagingEvent notifyListener:ptMetalGraphics->ptSharedEventListener
                        atValue:ptMetalGraphics->uNextFenceValue++
                        block:^(id<MTLSharedEvent> sharedEvent, uint64_t value) {
        if(pData)
            memcpy(ptMetalGraphics->tStagingBuffer.contents, pData, szSize);
        sharedEvent.signaledValue = ptMetalGraphics->uNextFenceValue;
    }];

    // wait for cpu to gpu copying to take place before continuing
    while(true)
    {
        if(ptMetalGraphics->tStagingEvent.signaledValue == ptMetalGraphics->uNextFenceValue)
            break;
    }

    id<MTLCommandBuffer> commandBuffer = [ptMetalGraphics->tCmdQueue commandBufferWithUnretainedReferences];
    commandBuffer.label = @"Heap Transfer Blit Encoder";

    [commandBuffer encodeWaitForEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue++];

    plMetalTexture tMetalTexture = ptMetalGraphics->sbtTexturesHot[tHandle.uIndex];
    plTexture tTexture = ptGraphics->sbtTexturesCold[tHandle.uIndex];

    id<MTLBlitCommandEncoder> blitEncoder = commandBuffer.blitCommandEncoder;
    blitEncoder.label = @"Heap Transfer Blit Encoder";

    NSUInteger uBytesPerRow = szSize / tTexture.tDesc.tDimensions.y;
    uBytesPerRow = uBytesPerRow / tTexture.tDesc.uLayers;
    MTLOrigin tOrigin;
    tOrigin.x = 0;
    tOrigin.y = 0;
    tOrigin.z = 0;
    MTLSize tSize;
    tSize.width = tTexture.tDesc.tDimensions.x;
    tSize.height = tTexture.tDesc.tDimensions.y;
    tSize.depth = tTexture.tDesc.tDimensions.z;
    for(uint32_t i = 0; i < tTexture.tDesc.uLayers; i++)
        [blitEncoder copyFromBuffer:ptMetalGraphics->tStagingBuffer sourceOffset:uBytesPerRow * tTexture.tDesc.tDimensions.y * i sourceBytesPerRow:uBytesPerRow sourceBytesPerImage:0 sourceSize:tSize toTexture:tMetalTexture.tTexture destinationSlice:i destinationLevel:0 destinationOrigin:tOrigin];

    if(tTexture.tDesc.uMips > 1)
        [blitEncoder generateMipmapsForTexture:tMetalTexture.tTexture];

    [blitEncoder endEncoding];
    [commandBuffer encodeSignalEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue];
    [commandBuffer commit];
}

static plTextureHandle
pl_create_texture(plDevice* ptDevice, plTextureDesc tDesc, size_t szSize, const void* pData, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uTextureIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtTextureFreeIndices) > 0)
        uTextureIndex = pl_sb_pop(ptGraphics->sbtTextureFreeIndices);
    else
    {
        uTextureIndex = pl_sb_size(ptGraphics->sbtTexturesCold);
        pl_sb_add(ptGraphics->sbtTexturesCold);
        pl_sb_push(ptGraphics->sbtTextureGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtTextureGenerations[uTextureIndex],
        .uIndex = uTextureIndex
    };

    if(tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;

    plTexture tTexture = {
        .tDesc = tDesc
    };

    // copy from cpu to gpu once staging buffer is free
    [ptMetalGraphics->tStagingEvent notifyListener:ptMetalGraphics->ptSharedEventListener
                        atValue:ptMetalGraphics->uNextFenceValue++
                        block:^(id<MTLSharedEvent> sharedEvent, uint64_t value) {
        if(pData)
            memcpy(ptMetalGraphics->tStagingBuffer.contents, pData, szSize);
        sharedEvent.signaledValue = ptMetalGraphics->uNextFenceValue;
    }];

    // wait for cpu to gpu copying to take place before continuing
    while(true)
    {
        if(ptMetalGraphics->tStagingEvent.signaledValue == ptMetalGraphics->uNextFenceValue)
            break;
    }


    MTLTextureDescriptor* ptTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    ptTextureDescriptor.pixelFormat = pl__metal_format(tDesc.tFormat);
    ptTextureDescriptor.width = tDesc.tDimensions.x;
    ptTextureDescriptor.height = tDesc.tDimensions.y;
    ptTextureDescriptor.mipmapLevelCount = tDesc.uMips;
    ptTextureDescriptor.storageMode = MTLStorageModePrivate;
    ptTextureDescriptor.arrayLength = 1;
    ptTextureDescriptor.depth = tDesc.tDimensions.z;
    ptTextureDescriptor.sampleCount = tDesc.tSamples;

    if(tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
        ptTextureDescriptor.usage |= MTLTextureUsageShaderRead;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;

    // if(tDesc.tUsage & PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT)
    //     ptTextureDescriptor.storageMode = MTLStorageModeMemoryless;

    if(tDesc.tSamples > 1)
        ptTextureDescriptor.textureType = MTLTextureType2DMultisample;
    else if(tDesc.tType == PL_TEXTURE_TYPE_2D)
        ptTextureDescriptor.textureType = MTLTextureType2D;
    else if(tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        ptTextureDescriptor.textureType = MTLTextureTypeCube;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    MTLSizeAndAlign tSizeAndAlign = [ptMetalDevice->tDevice heapTextureSizeAndAlignWithDescriptor:ptTextureDescriptor];
    plDeviceMemoryAllocatorI* ptAllocator = tSizeAndAlign.size > PL_DEVICE_ALLOCATION_BLOCK_SIZE ? &ptGraphics->tDevice.tLocalDedicatedAllocator : &ptGraphics->tDevice.tLocalBuddyAllocator;
    tTexture.tMemoryAllocation = ptAllocator->allocate(ptAllocator->ptInst, ptTextureDescriptor.storageMode, tSizeAndAlign.size, tSizeAndAlign.align, pcName);

    plMetalTexture tMetalTexture = {
        .tTexture = [(id<MTLHeap>)tTexture.tMemoryAllocation.uHandle newTextureWithDescriptor:ptTextureDescriptor offset:tTexture.tMemoryAllocation.ulOffset],
        .tHeap = (id<MTLHeap>)tTexture.tMemoryAllocation.uHandle
    };
    tMetalTexture.tTexture.label = [NSString stringWithUTF8String:pcName];

    if(pData)
    {
        id<MTLCommandBuffer> commandBuffer = [ptMetalGraphics->tCmdQueue commandBufferWithUnretainedReferences];
        commandBuffer.label = @"Heap Transfer Blit Encoder";

        [commandBuffer encodeWaitForEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue++];

        id<MTLBlitCommandEncoder> blitEncoder = commandBuffer.blitCommandEncoder;
        blitEncoder.label = @"Heap Transfer Blit Encoder";

        NSUInteger uBytesPerRow = szSize / tDesc.tDimensions.y;
        uBytesPerRow = uBytesPerRow / tDesc.uLayers;
        MTLOrigin tOrigin;
        tOrigin.x = 0;
        tOrigin.y = 0;
        tOrigin.z = 0;
        MTLSize tSize;
        tSize.width = tDesc.tDimensions.x;
        tSize.height = tDesc.tDimensions.y;
        tSize.depth = tDesc.tDimensions.z;
        for(uint32_t i = 0; i < tDesc.uLayers; i++)
            [blitEncoder copyFromBuffer:ptMetalGraphics->tStagingBuffer sourceOffset:uBytesPerRow * tDesc.tDimensions.y * i sourceBytesPerRow:uBytesPerRow sourceBytesPerImage:0 sourceSize:tSize toTexture:tMetalTexture.tTexture destinationSlice:i destinationLevel:0 destinationOrigin:tOrigin];

        if(tDesc.uMips > 1)
            [blitEncoder generateMipmapsForTexture:tMetalTexture.tTexture];

        [blitEncoder endEncoding];
        [commandBuffer encodeSignalEvent:ptMetalGraphics->tStagingEvent value:ptMetalGraphics->uNextFenceValue];
        [commandBuffer commit];
    }
    ptMetalGraphics->sbtTexturesHot[uTextureIndex] = tMetalTexture;
    ptGraphics->sbtTexturesCold[uTextureIndex] = tTexture;
    [ptTextureDescriptor release];
    return tHandle;
}


static plTextureViewHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const plSampler* ptSampler, plTextureHandle tTextureHandle, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plTexture* ptTexture = pl__get_texture(ptDevice, tTextureHandle);
    plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex];

    uint32_t uTextureViewIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtTextureViewFreeIndices) > 0)
        uTextureViewIndex = pl_sb_pop(ptGraphics->sbtTextureViewFreeIndices);
    else
    {
        uTextureViewIndex = pl_sb_size(ptGraphics->sbtTextureViewsCold);
        pl_sb_add(ptGraphics->sbtTextureViewsCold);
        pl_sb_push(ptGraphics->sbtTextureViewGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtSamplersHot);
    }

    plTextureViewHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtTextureViewGenerations[uTextureViewIndex],
        .uIndex = uTextureViewIndex
    };

    plTextureView tTextureView = {
        .tSampler         = *ptSampler,
        .tTextureViewDesc = *ptViewDesc,
        .tTexture         = tTextureHandle
    };

    if(ptViewDesc->uMips == 0)
        tTextureView.tTextureViewDesc.uMips = ptTexture->tDesc.uMips;

    MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
    samplerDesc.minFilter = pl__metal_filter(ptSampler->tFilter);
    samplerDesc.magFilter = pl__metal_filter(ptSampler->tFilter);
    samplerDesc.mipFilter = MTLSamplerMipFilterLinear;
    samplerDesc.normalizedCoordinates = YES;
    samplerDesc.supportArgumentBuffers = YES;
    samplerDesc.sAddressMode = pl__metal_wrap(ptSampler->tHorizontalWrap);
    samplerDesc.tAddressMode = pl__metal_wrap(ptSampler->tVerticalWrap);
    samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack;
    samplerDesc.compareFunction = pl__metal_compare(ptSampler->tCompare);
    samplerDesc.lodMinClamp = ptSampler->fMinMip;
    samplerDesc.lodMaxClamp = ptSampler->fMaxMip;
    samplerDesc.label = [NSString stringWithUTF8String:pcName];

    plMetalSampler tMetalSampler = {
        .tSampler = [ptMetalDevice->tDevice newSamplerStateWithDescriptor:samplerDesc]
    };

    ptMetalGraphics->sbtSamplersHot[uTextureViewIndex] = tMetalSampler;
    ptGraphics->sbtTextureViewsCold[uTextureViewIndex] = tTextureView;
    return tHandle;
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, plBindGroupLayout* ptLayout)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plBindGroupHandle tHandle = {
        .uGeneration = 0,
        .uIndex = pl_sb_size(ptMetalGraphics->sbtBindGroupsHot)
    };

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    NSUInteger argumentBufferLength = sizeof(MTLResourceID) * ptLayout->uTextureCount * 2 + sizeof(void*) * ptLayout->uBufferCount;

    plMetalBindGroup tMetalBindGroup = {
        .tShaderArgumentBuffer = [ptMetalDevice->tDevice newBufferWithLength:argumentBufferLength options:0]
    };

    pl_sb_push(ptMetalGraphics->sbtBindGroupsHot, tMetalBindGroup);
    pl_sb_push(ptGraphics->sbtBindGroupsCold, tBindGroup);
    pl_sb_push(ptGraphics->sbtBindGroupGenerations, 0);
    return tHandle;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle* ptGroup, uint32_t uBufferCount, plBufferHandle* atBuffers, size_t* aszBufferRanges, uint32_t uTextureViewCount, plTextureViewHandle* atTextureViews)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptGroup->uIndex];
    plBindGroup* ptBindGroup = &ptGraphics->sbtBindGroupsCold[ptGroup->uIndex];
    

    // start of buffers
    float** ptBufferResources = (float**)ptMetalBindGroup->tShaderArgumentBuffer.contents;
    for(uint32_t i = 0; i < uBufferCount; i++)
    {
        plMetalBuffer* ptMetalBuffer = &ptMetalGraphics->sbtBuffersHot[atBuffers[i].uIndex];
        ptBufferResources[i] = (float*)ptMetalBuffer->tBuffer.gpuAddress;
        ptBindGroup->tLayout.aBuffers[i].tBuffer = atBuffers[i];
    }

    // start of textures
    char* pcStartOfBuffers = (char*)ptMetalBindGroup->tShaderArgumentBuffer.contents;

    MTLResourceID* ptResources = (MTLResourceID*)(&pcStartOfBuffers[sizeof(void*) * uBufferCount]);
    for(uint32_t i = 0; i < uTextureViewCount; i++)
    {
        
        plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[ptGraphics->sbtTextureViewsCold[atTextureViews[i].uIndex].tTexture.uIndex];
        plMetalSampler* ptMetalSampler = &ptMetalGraphics->sbtSamplersHot[atTextureViews[i].uIndex];
        ptResources[i * 2] = ptMetalTexture->tTexture.gpuResourceID;
        ptResources[i * 2 + 1] = ptMetalSampler->tSampler.gpuResourceID;
        ptBindGroup->tLayout.aTextures[i].tTextureView = atTextureViews[i];
    }

    ptMetalBindGroup->tLayout = ptBindGroup->tLayout;
}

static plDynamicBinding
pl_allocate_dynamic_data(plDevice* ptDevice, size_t szSize)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    PL_ASSERT(szSize <= PL_MAX_DYNAMIC_DATA_SIZE && "Dynamic data size too large");

    plMetalDynamicBuffer* ptDynamicBuffer = NULL;

    // first call this frame
    if(ptFrame->uCurrentBufferIndex == UINT32_MAX)
    {
        ptFrame->uCurrentBufferIndex = 0;
        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[0];
        ptDynamicBuffer->uByteOffset = 0;
    }
    ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];

    // check if current block has room
    if(ptDynamicBuffer->uByteOffset + szSize > PL_DEVICE_ALLOCATION_BLOCK_SIZE)
    {
        ptFrame->uCurrentBufferIndex++;
        
        // check if we have available block
        if(ptFrame->uCurrentBufferIndex + 1 > pl_sb_size(ptFrame->sbtDynamicBuffers)) // create new buffer
        {
            // dynamic buffer stuff
            pl_sb_add(ptFrame->sbtDynamicBuffers);
            ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
            ptDynamicBuffer->uByteOffset = 0;
            static char atNameBuffer[PL_MAX_NAME_LENGTH] = {0};
            pl_sprintf(atNameBuffer, "D-BUF-F%d-%d", (int)ptGraphics->uCurrentFrameIndex, (int)ptFrame->uCurrentBufferIndex);

            ptDynamicBuffer->tMemory = ptGraphics->tDevice.tStagingUnCachedAllocator.allocate(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, 0, PL_DEVICE_ALLOCATION_BLOCK_SIZE, 0, atNameBuffer);
            ptDynamicBuffer->tBuffer = [(id<MTLHeap>)ptDynamicBuffer->tMemory.uHandle newBufferWithLength:PL_DEVICE_ALLOCATION_BLOCK_SIZE options:MTLResourceStorageModeShared offset:0];
        }

        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
        ptDynamicBuffer->uByteOffset = 0;
    }

    plDynamicBinding tDynamicBinding = {
        .uBufferHandle = ptFrame->uCurrentBufferIndex,
        .uByteOffset   = ptDynamicBuffer->uByteOffset,
        .pcData        = &ptDynamicBuffer->tBuffer.contents[ptDynamicBuffer->uByteOffset]
    };
    ptDynamicBuffer->uByteOffset = pl_align_up((size_t)ptDynamicBuffer->uByteOffset + PL_MAX_DYNAMIC_DATA_SIZE, 256);
    return tDynamicBinding;
}

static plShaderHandle
pl_create_shader(plDevice* ptDevice, plShaderDescription* ptDescription)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    plShader tShader = {
        .tDescription = *ptDescription,
    };

    plShaderHandle tHandle = {
        .uIndex      = pl_sb_size(ptMetalGraphics->sbtShadersHot),
        .uGeneration = 0
    };

    // read in shader source code
    unsigned uShaderFileSize = 0;
    gptFile->read(ptDescription->pcVertexShader, &uShaderFileSize, NULL, "rb");
    char* pcFileData = PL_ALLOC(uShaderFileSize + 1);
    gptFile->read(ptDescription->pcVertexShader, &uShaderFileSize, pcFileData, "rb");

    // prepare preprocessor defines
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];

    int iDataStride = 0;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_POSITION) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0) iDataStride += 4;
    if(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1) iDataStride += 4;

    int iTextureCount = 0;
    if(ptDescription->tGraphicsState.ulShaderTextureFlags & PL_SHADER_TEXTURE_FLAG_BINDING_0)
        iTextureCount++;

    ptCompileOptions.preprocessorMacros = @{
        @"PL_TEXTURE_COUNT" : [NSNumber numberWithInt:iTextureCount],
        @"PL_DATA_STRIDE" : [NSNumber numberWithInt:iDataStride],
        @"PL_MESH_FORMAT_FLAG_HAS_POSITION" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_POSITION)],
        @"PL_MESH_FORMAT_FLAG_HAS_NORMAL" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)],
        @"PL_MESH_FORMAT_FLAG_HAS_TANGENT" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT)],
        @"PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0)],
        @"PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1)],
        @"PL_MESH_FORMAT_FLAG_HAS_COLOR_0" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0)],
        @"PL_MESH_FORMAT_FLAG_HAS_COLOR_1" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1)],
        @"PL_MESH_FORMAT_FLAG_HAS_JOINTS_0" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0)],
        @"PL_MESH_FORMAT_FLAG_HAS_JOINTS_1" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1)],
        @"PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0)],
        @"PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1" : [NSNumber numberWithInt:(ptDescription->tGraphicsState.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1)]
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
    pipelineDescriptor.rasterSampleCount = ptGraphics->sbtRenderPassesCold[ptDescription->tRenderPass.uIndex].tSampleCount;

    // renderpass stuff
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[ptDescription->tRenderPass.uIndex];
    const plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptGraphics->sbtRenderPassesCold[ptDescription->tRenderPass.uIndex].tDesc.tLayout.uIndex];

    MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual;
    depthDescriptor.depthWriteEnabled = ptDescription->tGraphicsState.ulDepthWriteEnabled ? YES : NO;

    pipelineDescriptor.colorAttachments[0].pixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[0].tFormat);
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
    pipelineDescriptor.depthAttachmentPixelFormat = pl__metal_format(ptLayout->tDesc.tDepthTarget.tFormat);
    pipelineDescriptor.stencilAttachmentPixelFormat = ptMetalRenderPass->ptRenderPassDescriptor.stencilAttachment.texture.pixelFormat;

    const plMetalShader tMetalShader = {
        .tDepthStencilState   = [ptMetalDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor],
        .tRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error],
        .tCullMode            = pl__metal_cull(ptDescription->tGraphicsState.ulCullMode)
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    pl_sb_push(ptMetalGraphics->sbtShadersHot, tMetalShader);
    pl_sb_push(ptGraphics->sbtShadersCold, tShader);
    pl_sb_push(ptGraphics->sbtShaderGenerations, 0);

    PL_FREE(pcFileData);
    return tHandle;
}

static void
pl_initialize_graphics(plGraphics* ptGraphics)
{
    plIO* ptIOCtx = pl_get_io();

    ptGraphics->_pInternalData = PL_ALLOC(sizeof(plGraphicsMetal));
    memset(ptGraphics->_pInternalData, 0, sizeof(plGraphicsMetal));

    ptGraphics->tDevice._pInternalData = PL_ALLOC(sizeof(plDeviceMetal));
    memset(ptGraphics->tDevice._pInternalData, 0, sizeof(plDeviceMetal));

    ptGraphics->tSwapchain._pInternalData = PL_ALLOC(sizeof(plMetalSwapchain));
    memset(ptGraphics->tSwapchain._pInternalData, 0, sizeof(plMetalSwapchain));

    ptGraphics->tDevice.ptGraphics = ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    ptMetalDevice->tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    ptGraphics->uFramesInFlight = 2;
    ptGraphics->tSwapchain.uImageCount = 2;
    ptGraphics->tSwapchain.tFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptGraphics->tSwapchain.tDepthFormat = PL_FORMAT_D32_FLOAT;
    pl_sb_resize(ptGraphics->tSwapchain.sbtSwapchainTextureViews, 2);

    // create command queue
    ptMetalGraphics->tCmdQueue = [ptMetalDevice->tDevice newCommandQueue];

    ptGraphics->tSwapchain.tMsaaSamples = 4;
    if([ptMetalDevice->tDevice supportsTextureSampleCount:8])
        ptGraphics->tSwapchain.tMsaaSamples = 8;

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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~device memory allocators~~~~~~~~~~~~~~~~~~~~~~~~~

    // local dedicated
    static plDeviceAllocatorData tLocalDedicatedData = {0};
    tLocalDedicatedData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tLocalDedicatedAllocator.allocate = pl_allocate_dedicated;
    ptGraphics->tDevice.tLocalDedicatedAllocator.free = pl_free_dedicated;
    ptGraphics->tDevice.tLocalDedicatedAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tLocalDedicatedAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tLocalDedicatedData;

    // local buddy
    static plDeviceAllocatorData tLocalBuddyData = {0};
    for(uint32_t i = 0; i < PL_DEVICE_LOCAL_LEVELS; i++)
        tLocalBuddyData.auFreeList[i] = UINT32_MAX;
    tLocalBuddyData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tLocalBuddyAllocator.allocate = pl_allocate_buddy;
    ptGraphics->tDevice.tLocalBuddyAllocator.free = pl_free_buddy;
    ptGraphics->tDevice.tLocalBuddyAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tLocalBuddyAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tLocalBuddyAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tLocalBuddyData;

    // staging uncached
    static plDeviceAllocatorData tStagingUncachedData = {0};
    tStagingUncachedData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tStagingUnCachedAllocator.allocate = pl_allocate_staging_uncached;
    ptGraphics->tDevice.tStagingUnCachedAllocator.free = pl_free_staging_uncached;
    ptGraphics->tDevice.tStagingUnCachedAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tStagingUnCachedAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tStagingUncachedData;

    ptMetalGraphics->tStagingEvent = [ptMetalDevice->tDevice newSharedEvent];
    dispatch_queue_t tQueue = dispatch_queue_create("com.example.apple-samplecode.MyQueue", NULL);
    ptMetalGraphics->ptSharedEventListener = [[MTLSharedEventListener alloc] initWithDispatchQueue:tQueue];

    pl_sb_resize(ptGraphics->sbtGarbage, ptGraphics->uFramesInFlight);
    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {
            .tFrameBoundarySemaphore = dispatch_semaphore_create(1)
        };
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        static char atNameBuffer[PL_MAX_NAME_LENGTH] = {0};
        pl_sprintf(atNameBuffer, "D-BUF-F%d-0", (int)i);
        tFrame.sbtDynamicBuffers[0].tMemory = ptGraphics->tDevice.tStagingUnCachedAllocator.allocate(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, 0, PL_DEVICE_ALLOCATION_BLOCK_SIZE, 0,atNameBuffer);
        tFrame.sbtDynamicBuffers[0].tBuffer = [(id<MTLHeap>)tFrame.sbtDynamicBuffers[0].tMemory.uHandle newBufferWithLength:PL_DEVICE_ALLOCATION_BLOCK_SIZE options:MTLResourceStorageModeShared offset:0];
        pl_sb_push(ptMetalGraphics->sbFrames, tFrame);
    }

    ptMetalGraphics->tStagingMemory = ptGraphics->tDevice.tStagingUnCachedAllocator.allocate(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, 0, PL_DEVICE_ALLOCATION_BLOCK_SIZE, 0, "staging");
    ptMetalGraphics->tStagingBuffer = [(id<MTLHeap>)ptMetalGraphics->tStagingMemory.uHandle newBufferWithLength:PL_DEVICE_ALLOCATION_BLOCK_SIZE options:MTLResourceStorageModeShared offset:0];

    // color & depth
    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], 1},
        .tFormat = PL_FORMAT_D32_FLOAT,
        .uLayers = 1,
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_2D,
        .tUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT,
        .tSamples = ptGraphics->tSwapchain.tMsaaSamples
    };
    ptGraphics->tSwapchain.tDepthTexture = pl_create_texture(&ptGraphics->tDevice, tDepthTextureDesc, 0, NULL, "Swapchain depth");

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], 1},
        .tFormat = PL_FORMAT_B8G8R8A8_UNORM,
        .uLayers = 1,
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_2D,
        .tUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .tSamples = ptGraphics->tSwapchain.tMsaaSamples
    };
    ptGraphics->tSwapchain.tColorTexture = pl_create_texture(&ptGraphics->tDevice, tColorTextureDesc, 0, NULL, "Swapchain color");

    plSampler tSampler = {
        .tFilter = PL_FILTER_NEAREST,
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tVerticalWrap = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };

    plTextureViewDesc tColorTextureViewDesc = {
        .tFormat     = tColorTextureDesc.tFormat,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    ptGraphics->tSwapchain.tColorTextureView = pl_create_texture_view(&ptGraphics->tDevice, &tColorTextureViewDesc, &tSampler, ptGraphics->tSwapchain.tColorTexture, "Swapchain color view");

    plTextureViewDesc tDepthTextureViewDesc = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    ptGraphics->tSwapchain.tDepthTextureView = pl_create_texture_view(&ptGraphics->tDevice, &tDepthTextureViewDesc, &tSampler, ptGraphics->tSwapchain.tDepthTexture, "Swapchain depth view");

}

static void
pl_setup_ui(plGraphics* ptGraphics, plRenderPassHandle tPass)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    pl_initialize_metal(ptMetalDevice->tDevice);
}

static void
pl_resize(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plIO* ptIOCtx = pl_get_io();

    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    // recreate depth texture

    pl_submit_texture_for_deletion(&ptGraphics->tDevice, ptGraphics->tSwapchain.tColorTexture);
    pl_submit_texture_for_deletion(&ptGraphics->tDevice, ptGraphics->tSwapchain.tDepthTexture);
    pl_submit_texture_view_for_deletion(&ptGraphics->tDevice, ptGraphics->tSwapchain.tColorTextureView);
    pl_submit_texture_view_for_deletion(&ptGraphics->tDevice, ptGraphics->tSwapchain.tDepthTextureView);

    // color & depth
    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], 1},
        .tFormat = PL_FORMAT_D32_FLOAT,
        .uLayers = 1,
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_2D,
        .tUsage = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT | PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT,
        .tSamples = ptGraphics->tSwapchain.tMsaaSamples
    };
    ptGraphics->tSwapchain.tDepthTexture = pl_create_texture(&ptGraphics->tDevice, tDepthTextureDesc, 0, NULL, "Swapchain depth");

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], 1},
        .tFormat = PL_FORMAT_B8G8R8A8_UNORM,
        .uLayers = 1,
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_2D,
        .tUsage = PL_TEXTURE_USAGE_COLOR_ATTACHMENT | PL_TEXTURE_USAGE_SAMPLED,
        .tSamples = ptGraphics->tSwapchain.tMsaaSamples
    };
    ptGraphics->tSwapchain.tColorTexture = pl_create_texture(&ptGraphics->tDevice, tColorTextureDesc, 0, NULL, "Swapchain color");

    plSampler tSampler = {
        .tFilter = PL_FILTER_NEAREST,
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tVerticalWrap = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };

    plTextureViewDesc tColorTextureViewDesc = {
        .tFormat     = tColorTextureDesc.tFormat,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    ptGraphics->tSwapchain.tColorTextureView = pl_create_texture_view(&ptGraphics->tDevice, &tColorTextureViewDesc, &tSampler, ptGraphics->tSwapchain.tColorTexture, "Swapchain color view");

    plTextureViewDesc tDepthTextureViewDesc = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    ptGraphics->tSwapchain.tDepthTextureView = pl_create_texture_view(&ptGraphics->tDevice, &tDepthTextureViewDesc, &tSampler, ptGraphics->tSwapchain.tDepthTexture, "Swapchain depth view");

    pl_end_profile_sample();
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    // Wait until the inflight command buffer has completed its work
    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
    ptGraphics->tSwapchain.uCurrentImageIndex = ptGraphics->uCurrentFrameIndex;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    dispatch_semaphore_wait(ptFrame->tFrameBoundarySemaphore, DISPATCH_TIME_FOREVER);
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtFrameBuffers); i++)
    {
        const uint32_t iBufferIndex = ptGarbage->sbtFrameBuffers[i].uIndex;
        pl_sb_push(ptGraphics->sbtFrameBufferFreeIndices, iBufferIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint32_t uTextureIndex = ptGarbage->sbtTextures[i].uIndex;
        plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[uTextureIndex];
        [ptMetalTexture->tTexture release];
        ptMetalTexture->tTexture = nil;
        pl_sb_push(ptGraphics->sbtTextureFreeIndices, uTextureIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextureViews); i++)
    {
        const uint32_t uTextureViewIndex = ptGarbage->sbtTextureViews[i].uIndex;
        plMetalSampler* ptMetalSampler = &ptMetalGraphics->sbtSamplersHot[uTextureViewIndex];
        [ptMetalSampler->tSampler release];
        ptMetalSampler->tSampler = nil;
        pl_sb_push(ptGraphics->sbtTextureViewFreeIndices, uTextureViewIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBuffers); i++)
    {
        const uint32_t iBufferIndex = ptGarbage->sbtBuffers[i].uIndex;
        [ptMetalGraphics->sbtBuffersHot[iBufferIndex].tBuffer release];
        ptMetalGraphics->sbtBuffersHot[iBufferIndex].tBuffer = nil;
        pl_sb_push(ptGraphics->sbtBufferFreeIndices, iBufferIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtMemory); i++)
    {
        if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tLocalBuddyAllocator.ptInst)
            ptGraphics->tDevice.tLocalBuddyAllocator.free(ptGraphics->tDevice.tLocalBuddyAllocator.ptInst, &ptGarbage->sbtMemory[i]);
        else if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst)
            ptGraphics->tDevice.tLocalDedicatedAllocator.free(ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst, &ptGarbage->sbtMemory[i]);
        else if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst)
            ptGraphics->tDevice.tStagingUnCachedAllocator.free(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, &ptGarbage->sbtMemory[i]);
    }

    pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtMemory);
    pl_sb_reset(ptGarbage->sbtBuffers);
    pl_sb_reset(ptGarbage->sbtTextureViews);
    pl_sb_reset(ptGarbage->sbtFrameBuffers);
    
    plIO* ptIOCtx = pl_get_io();
    ptMetalGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;
    
    // get next drawable
    ptMetalGraphics->tCurrentDrawable = [ptMetalGraphics->pMetalLayer nextDrawable];

    if(!ptMetalGraphics->tCurrentDrawable)
    {
        pl_end_profile_sample();
        return false;
    }

    ptMetalGraphics->tCurrentCommandBuffer = [ptMetalGraphics->tCmdQueue commandBufferWithUnretainedReferences];

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

static bool
pl_end_gfx_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;

    [ptMetalGraphics->tCurrentCommandBuffer presentDrawable:ptMetalGraphics->tCurrentDrawable];

    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    ptFrame->uCurrentBufferIndex = UINT32_MAX;

    dispatch_semaphore_t semaphore = ptFrame->tFrameBoundarySemaphore;
    [ptMetalGraphics->tCurrentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
        // GPU work is complete
        // Signal the semaphore to start the CPU work
        dispatch_semaphore_signal(semaphore);
    }];

    [ptMetalGraphics->tCurrentCommandBuffer commit];

    pl_end_profile_sample();
    return true;
}

static void
pl_begin_recording(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    pl_end_profile_sample();
}

static void
pl_end_recording(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    pl_end_profile_sample();
}

static void
pl_begin_main_pass(plGraphics* ptGraphics, plFrameBufferHandle tFrameBufferHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plFrameBuffer* ptFrameBuffer = &ptGraphics->sbtFrameBuffersCold[tFrameBufferHandle.uIndex];
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[ptFrameBuffer->tDescription.tRenderPass.uIndex];

    const uint32_t uColorIndex = ptGraphics->sbtTextureViewsCold[ptFrameBuffer->tDescription.atViewAttachments[0].uIndex].tTexture.uIndex;
    const uint32_t uDepthIndex = ptGraphics->sbtTextureViewsCold[ptFrameBuffer->tDescription.atViewAttachments[1].uIndex].tTexture.uIndex;
    ptMetalRenderPass->ptRenderPassDescriptor.depthAttachment.texture = ptMetalGraphics->sbtTexturesHot[uDepthIndex].tTexture;
    ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].texture = ptMetalGraphics->sbtTexturesHot[uColorIndex].tTexture;
    ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].resolveTexture = ptMetalGraphics->tCurrentDrawable.texture;
    ptMetalGraphics->tCurrentRenderEncoder = [ptMetalGraphics->tCurrentCommandBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->ptRenderPassDescriptor];

    pl_new_draw_frame_metal(ptMetalRenderPass->ptRenderPassDescriptor);
    pl_end_profile_sample();
}

static void
pl_end_main_pass(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    [ptMetalGraphics->tCurrentRenderEncoder endEncoding];
    pl_end_profile_sample();
}

static void
pl_begin_pass(plGraphics* ptGraphics, plFrameBufferHandle tFrameBufferHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plFrameBuffer* ptFrameBuffer = &ptGraphics->sbtFrameBuffersCold[tFrameBufferHandle.uIndex];
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[ptFrameBuffer->tDescription.tRenderPass.uIndex];

    const uint32_t uColorIndex = ptGraphics->sbtTextureViewsCold[ptFrameBuffer->tDescription.atViewAttachments[0].uIndex].tTexture.uIndex;
    const uint32_t uDepthIndex = ptGraphics->sbtTextureViewsCold[ptFrameBuffer->tDescription.atViewAttachments[1].uIndex].tTexture.uIndex;
    ptMetalRenderPass->ptRenderPassDescriptor.depthAttachment.texture = ptMetalGraphics->sbtTexturesHot[uDepthIndex].tTexture;
    ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].texture = ptMetalGraphics->sbtTexturesHot[uColorIndex].tTexture;
    ptMetalGraphics->tCurrentRenderEncoder = [ptMetalGraphics->tCurrentCommandBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->ptRenderPassDescriptor];

    pl_end_profile_sample();
}

static void
pl_end_pass(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    [ptMetalGraphics->tCurrentRenderEncoder endEncoding];
    pl_end_profile_sample();
}

static void
pl_draw_areas(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLDevice> tDevice = ptMetalDevice->tDevice;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];
        plDrawStream* ptStream = ptArea->ptDrawStream;

        MTLScissorRect tScissorRect = {
            .x      = (NSUInteger)(ptArea->tScissor.iOffsetX),
            .y      = (NSUInteger)(ptArea->tScissor.iOffsetY),
            .width  = (NSUInteger)(ptArea->tScissor.uWidth),
            .height = (NSUInteger)(ptArea->tScissor.uHeight)
        };
        [ptMetalGraphics->tCurrentRenderEncoder setScissorRect:tScissorRect];

        MTLViewport tViewport = {
            .originX = ptArea->tViewport.fX,
            .originY = ptArea->tViewport.fY,
            .width   = ptArea->tViewport.fWidth,
            .height  = ptArea->tViewport.fHeight
        };
        [ptMetalGraphics->tCurrentRenderEncoder setViewport:tViewport];

        const uint32_t uTokens = pl_sb_size(ptStream->sbtStream);
        uint32_t uCurrentStreamIndex = 0;
        uint32_t uTriangleCount = 0;
        uint32_t uIndexBuffer = 0;
        uint32_t uIndexBufferOffset = 0;
        uint32_t uVertexBufferOffset = 0;
        uint32_t uDynamicBufferOffset = 0;

        while(uCurrentStreamIndex < uTokens)
        {
            const uint32_t uDirtyMask = ptStream->sbtStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                plMetalShader* ptMetalShader = &ptMetalGraphics->sbtShadersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                [ptMetalGraphics->tCurrentRenderEncoder setCullMode:ptMetalShader->tCullMode];
                [ptMetalGraphics->tCurrentRenderEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
                [ptMetalGraphics->tCurrentRenderEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uBufferCount; k++)
                {
                    const plBufferHandle tBufferHandle = ptMetalBindGroup->tLayout.aBuffers[k].tBuffer;
                    [ptMetalGraphics->tCurrentRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tHeap stages:MTLRenderStageVertex];
                    [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer
                        usage:MTLResourceUsageRead
                        stages:MTLRenderStageVertex]; 
                }


                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureCount; k++)
                {
                    
                    const plTextureHandle tTextureHandle = ptGraphics->sbtTextureViewsCold[ptMetalBindGroup->tLayout.aTextures[k].tTextureView.uIndex].tTexture;
                    id<MTLHeap> tHeap = ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tHeap;
                    [ptMetalGraphics->tCurrentRenderEncoder useHeap:tHeap stages:MTLRenderStageFragment];
                    [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture
                        usage:MTLResourceUsageRead
                        stages:MTLRenderStageFragment];  
                }

                [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:0
                    atIndex:1];

                [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:0
                    atIndex:1];

                uCurrentStreamIndex++;
            }
     
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uBufferCount; k++)
                {
                    const plBufferHandle tBufferHandle = ptMetalBindGroup->tLayout.aBuffers[k].tBuffer;
                    [ptMetalGraphics->tCurrentRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tHeap stages:MTLRenderStageVertex];
                    [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer
                        usage:MTLResourceUsageRead
                        stages:MTLRenderStageVertex]; 
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptGraphics->sbtTextureViewsCold[ptMetalBindGroup->tLayout.aTextures[k].tTextureView.uIndex].tTexture;
                    id<MTLHeap> tHeap = ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tHeap;
                    [ptMetalGraphics->tCurrentRenderEncoder useHeap:tHeap stages:MTLRenderStageFragment];
                    [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture
                        usage:MTLResourceUsageRead
                        stages:MTLRenderStageFragment];  
                }

                [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:0
                    atIndex:2];

                [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:0
                    atIndex:2];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uBufferCount; k++)
                {
                    const plBufferHandle tBufferHandle = ptMetalBindGroup->tLayout.aBuffers[k].tBuffer;
                    [ptMetalGraphics->tCurrentRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tHeap stages:MTLRenderStageVertex];
                    [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer
                        usage:MTLResourceUsageRead
                        stages:MTLRenderStageVertex]; 
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptGraphics->sbtTextureViewsCold[ptMetalBindGroup->tLayout.aTextures[k].tTextureView.uIndex].tTexture;
                    id<MTLHeap> tHeap = ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tHeap;
                    [ptMetalGraphics->tCurrentRenderEncoder useHeap:tHeap stages:MTLRenderStageFragment];
                    [ptMetalGraphics->tCurrentRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture
                        usage:MTLResourceUsageRead
                        stages:MTLRenderStageFragment]; 
                }

                [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:0
                    atIndex:3];

                [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:0
                    atIndex:3];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
            {
                uIndexBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
            {
                uVertexBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                uDynamicBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
            {

                [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer
                    offset:0
                    atIndex:4];

                [ptMetalGraphics->tCurrentRenderEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer
                    offset:0
                    atIndex:4];

                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                [ptMetalGraphics->tCurrentRenderEncoder setVertexBufferOffset:uDynamicBufferOffset atIndex:4];
                [ptMetalGraphics->tCurrentRenderEncoder setFragmentBufferOffset:uDynamicBufferOffset atIndex:4];
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
            {
                [ptMetalGraphics->tCurrentRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]].tHeap stages:MTLRenderStageVertex];
                uIndexBuffer = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
            {
                [ptMetalGraphics->tCurrentRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]].tHeap stages:MTLRenderStageVertex];
                [ptMetalGraphics->tCurrentRenderEncoder setVertexBuffer:ptMetalGraphics->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer
                    offset:0
                    atIndex:0];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
            {
                uTriangleCount = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            [ptMetalGraphics->tCurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
                indexCount:uTriangleCount * 3
                indexType:MTLIndexTypeUInt32
                indexBuffer:ptMetalGraphics->sbtBuffersHot[uIndexBuffer].tBuffer
                indexBufferOffset:uIndexBufferOffset * sizeof(uint32_t)
                instanceCount:1
                baseVertex:uVertexBufferOffset
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

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->sbtGarbage); i++)
    {
        plFrameGarbage* ptGarbage = &ptGraphics->sbtGarbage[i];
        plFrameContext* ptFrame = &ptMetalGraphics->sbFrames[i];
        pl_sb_free(ptGarbage->sbtMemory);
        pl_sb_free(ptGarbage->sbtTextures);
        pl_sb_free(ptGarbage->sbtTextureViews);
        pl_sb_free(ptGarbage->sbtBuffers);
        pl_sb_free(ptGarbage->sbtFrameBuffers);
        pl_sb_free(ptFrame->sbtDynamicBuffers);
    }

    pl_sb_free(ptGraphics->sbtGarbage);
    pl_sb_free(ptMetalGraphics->sbFrames);
    pl_sb_free(ptMetalGraphics->sbtTexturesHot);
    pl_sb_free(ptMetalGraphics->sbtSamplersHot);
    pl_sb_free(ptMetalGraphics->sbtBindGroupsHot);
    pl_sb_free(ptMetalGraphics->sbtBuffersHot);
    pl_sb_free(ptMetalGraphics->sbtShadersHot);

    plDeviceAllocatorData* ptData0 = (plDeviceAllocatorData*)ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst;
    plDeviceAllocatorData* ptData1 = (plDeviceAllocatorData*)ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst;
    plDeviceAllocatorData* ptData2 = (plDeviceAllocatorData*)ptGraphics->tDevice.tLocalBuddyAllocator.ptInst;
    pl_sb_free(ptData0->sbtBlocks);
    pl_sb_free(ptData1->sbtBlocks);
    pl_sb_free(ptData2->sbtBlocks);

    pl_sb_free(ptData0->sbtNodes);
    pl_sb_free(ptData1->sbtNodes);
    pl_sb_free(ptData2->sbtNodes);

    pl_sb_free(ptData0->sbtFreeBlockIndices);
    pl_sb_free(ptData1->sbtFreeBlockIndices);
    pl_sb_free(ptData2->sbtFreeBlockIndices);

    pl_sb_free(ptMetalGraphics->sbtPipelineEntries);
    pl_sb_free(ptMetalGraphics->sbFrames);
    pl_sb_free(ptMetalGraphics->sbtRenderPassesHot);
    pl_sb_free(ptGraphics->tSwapchain.sbtSwapchainTextureViews);
    pl_sb_free(ptGraphics->sbtShadersCold);
    pl_sb_free(ptGraphics->sbtBuffersCold);
    pl_sb_free(ptGraphics->sbtBufferFreeIndices);
    pl_sb_free(ptGraphics->sbtTexturesCold);
    pl_sb_free(ptGraphics->sbtTextureViewsCold);
    pl_sb_free(ptGraphics->sbtBindGroupsCold);
    pl_sb_free(ptGraphics->sbtShaderGenerations);
    pl_sb_free(ptGraphics->sbtBufferGenerations);
    pl_sb_free(ptGraphics->sbtTextureGenerations);
    pl_sb_free(ptGraphics->sbtTextureViewGenerations);
    pl_sb_free(ptGraphics->sbtBindGroupGenerations);
    pl_sb_free(ptGraphics->sbtFrameBufferFreeIndices);
    pl_sb_free(ptGraphics->sbtFrameBuffersCold);
    pl_sb_free(ptGraphics->sbtFrameBufferGenerations);
    pl_sb_free(ptGraphics->sbtRenderPassesCold);
    pl_sb_free(ptGraphics->sbtRenderPassGenerations);
    pl_sb_free(ptGraphics->sbtTextureFreeIndices);
    pl_sb_free(ptGraphics->sbtTextureViewFreeIndices);
    PL_FREE(ptGraphics->_pInternalData);
    PL_FREE(ptGraphics->tDevice._pInternalData);
    PL_FREE(ptGraphics->tSwapchain._pInternalData);
}

static void
pl_draw_lists(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists, plRenderPassHandle tPass)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLDevice> tDevice = ptMetalDevice->tDevice;
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[tPass.uIndex];

    plIO* ptIOCtx = pl_get_io();
    for(uint32_t i = 0; i < uListCount; i++)
    {
        pl_submit_metal_drawlist(&atLists[i], ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], ptMetalGraphics->tCurrentRenderEncoder, ptMetalGraphics->tCurrentCommandBuffer, ptMetalRenderPass->ptRenderPassDescriptor);
    }
}

static void
pl__submit_3d_drawlist(plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags, plRenderPassHandle tPass, uint32_t uMSAASampleCount)
{
    plGraphics* ptGfx = ptDrawlist->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGfx->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptGfx->tDevice._pInternalData;

    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[tPass.uIndex];
    plMetalPipelineEntry* ptPipelineEntry = pl__get_3d_pipelines(ptGfx, tFlags, ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].texture.sampleCount, ptMetalRenderPass->ptRenderPassDescriptor);

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

static MTLLoadAction
pl__metal_load_op(plLoadOp tOp)
{
    switch(tOp)
    {
        case PL_LOAD_OP_LOAD:      return MTLLoadActionLoad;
        case PL_LOAD_OP_CLEAR:     return MTLLoadActionClear;
        case PL_LOAD_OP_DONT_CARE: return MTLLoadActionDontCare;
    }

    PL_ASSERT(false && "Unsupported load op");
    return MTLLoadActionDontCare;
}

static MTLStoreAction
pl__metal_store_op(plStoreOp tOp)
{
    switch(tOp)
    {
        case PL_STORE_OP_STORE:               return MTLStoreActionStore;
        case PL_STORE_OP_MULTISAMPLE_RESOLVE: return MTLStoreActionMultisampleResolve;
        case PL_STORE_OP_DONT_CARE:           return MTLStoreActionDontCare;
        case PL_STORE_OP_NONE:                return MTLStoreActionUnknown;
    }

    PL_ASSERT(false && "Unsupported store op");
    return MTLStoreActionUnknown;
}

static MTLSamplerMinMagFilter
pl__metal_filter(plFilter tFilter)
{
    switch(tFilter)
    {
        case PL_FILTER_UNSPECIFIED:
        case PL_FILTER_NEAREST: return MTLSamplerMinMagFilterNearest;
        case PL_FILTER_LINEAR:  return MTLSamplerMinMagFilterLinear;
    }

    PL_ASSERT(false && "Unsupported filter mode");
    return MTLSamplerMinMagFilterLinear;
}

static MTLSamplerAddressMode
pl__metal_wrap(plWrapMode tWrap)
{
    switch(tWrap)
    {
        case PL_WRAP_MODE_UNSPECIFIED:
        case PL_WRAP_MODE_WRAP:   return MTLSamplerAddressModeMirrorRepeat;
        case PL_WRAP_MODE_CLAMP:  return MTLSamplerAddressModeClampToEdge;
        case PL_WRAP_MODE_MIRROR: return MTLSamplerAddressModeMirrorRepeat;
    }

    PL_ASSERT(false && "Unsupported wrap mode");
    return MTLSamplerAddressModeMirrorRepeat;
}

static MTLCompareFunction
pl__metal_compare(plCompareMode tCompare)
{
    switch(tCompare)
    {
        case PL_COMPARE_MODE_UNSPECIFIED:
        case PL_COMPARE_MODE_NEVER:            return MTLCompareFunctionNever;
        case PL_COMPARE_MODE_LESS:             return MTLCompareFunctionLess;
        case PL_COMPARE_MODE_EQUAL:            return MTLCompareFunctionEqual;
        case PL_COMPARE_MODE_LESS_OR_EQUAL:    return MTLCompareFunctionLessEqual;
        case PL_COMPARE_MODE_GREATER:          return MTLCompareFunctionGreater;
        case PL_COMPARE_MODE_NOT_EQUAL:        return MTLCompareFunctionNotEqual;
        case PL_COMPARE_MODE_GREATER_OR_EQUAL: return MTLCompareFunctionGreaterEqual;
        case PL_COMPARE_MODE_ALWAYS:           return MTLCompareFunctionAlways;
    }

    PL_ASSERT(false && "Unsupported compare mode");
    return MTLCompareFunctionNever;
}

static MTLPixelFormat
pl__metal_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_R32G32B32A32_FLOAT: return MTLPixelFormatRGBA32Float;
        case PL_FORMAT_R8G8B8A8_UNORM:     return MTLPixelFormatRGBA8Unorm;
        case PL_FORMAT_R32G32_FLOAT:       return MTLPixelFormatRG32Float;
        case PL_FORMAT_R8G8B8A8_SRGB:      return MTLPixelFormatRGBA8Unorm_sRGB;
        case PL_FORMAT_B8G8R8A8_SRGB:      return MTLPixelFormatBGRA8Unorm_sRGB;
        case PL_FORMAT_B8G8R8A8_UNORM:     return MTLPixelFormatBGRA8Unorm;
        case PL_FORMAT_D32_FLOAT:          return MTLPixelFormatDepth32Float;
        case PL_FORMAT_D32_FLOAT_S8_UINT:  return MTLPixelFormatDepth32Float_Stencil8;
        case PL_FORMAT_D24_UNORM_S8_UINT:  return MTLPixelFormatDepth24Unorm_Stencil8;
    }

    PL_ASSERT(false && "Unsupported format");
    return MTLPixelFormatInvalid;
}

static MTLCullMode
pl__metal_cull(plCullMode tCullMode)
{
    switch(tCullMode)
    {
        case PL_CULL_MODE_NONE:       return MTLCullModeNone;
        case PL_CULL_MODE_CULL_BACK:  return MTLCullModeBack;
        case PL_CULL_MODE_CULL_FRONT: return MTLCullModeFront;
    }
    PL_ASSERT(false && "Unsupported cull mode");
    return MTLCullModeNone;
};

static plTrackedMetalBuffer*
pl__dequeue_reusable_buffer(plGraphics* ptGraphics, NSUInteger length)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    double now = pl_get_io()->dTime;

    @synchronized(ptMetalGraphics->bufferCache)
    {
        // Purge old buffers that haven't been useful for a while
        if (now - ptMetalGraphics->lastBufferCachePurge > 1.0)
        {
            NSMutableArray* survivors = [NSMutableArray array];
            for (plTrackedMetalBuffer* candidate in ptMetalGraphics->bufferCache)
                if (candidate.lastReuseTime > ptMetalGraphics->lastBufferCachePurge)
                    [survivors addObject:candidate];
                else
                {
                    [candidate.buffer setPurgeableState:MTLPurgeableStateEmpty];
                    [candidate.buffer release];
                    [candidate release];
                }
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
pl__get_3d_pipelines(plGraphics* ptGraphics, pl3DDrawFlags tFlags, uint32_t uSampleCount, MTLRenderPassDescriptor* ptRenderPassDescriptor)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    for(uint32_t i = 0; i < pl_sb_size(ptMetalGraphics->sbtPipelineEntries); i++)
    {
        if(ptMetalGraphics->sbtPipelineEntries[i].tFlags == tFlags && ptMetalGraphics->sbtPipelineEntries[i].uSampleCount == uSampleCount)
            return &ptMetalGraphics->sbtPipelineEntries[i];
    }

    // pipeline not found, make new one

    plMetalPipelineEntry tPipelineEntry = {
        .tFlags = tFlags,
        .uSampleCount = uSampleCount
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
        pipelineDescriptor.rasterSampleCount = uSampleCount;

        pipelineDescriptor.colorAttachments[0].pixelFormat = ptRenderPassDescriptor.colorAttachments[0].texture.pixelFormat;
        pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
        pipelineDescriptor.depthAttachmentPixelFormat = ptRenderPassDescriptor.depthAttachment.texture.pixelFormat;
        pipelineDescriptor.stencilAttachmentPixelFormat = ptRenderPassDescriptor.stencilAttachment.texture.pixelFormat;

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
        pipelineDescriptor.rasterSampleCount = uSampleCount;
        pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
        pipelineDescriptor.depthAttachmentPixelFormat = ptRenderPassDescriptor.depthAttachment.texture.pixelFormat;
        pipelineDescriptor.stencilAttachmentPixelFormat = ptRenderPassDescriptor.stencilAttachment.texture.pixelFormat;

        tPipelineEntry.tSolidRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
        if (error != nil)
            NSLog(@"Error: failed to create Metal pipeline state: %@", error);
    }

    pl_sb_push(ptMetalGraphics->sbtPipelineEntries, tPipelineEntry);
    return &ptMetalGraphics->sbtPipelineEntries[pl_sb_size(ptMetalGraphics->sbtPipelineEntries) - 1];
}

//-----------------------------------------------------------------------------
// [SECTION] device memory allocators
//-----------------------------------------------------------------------------

static plDeviceMemoryAllocation
pl_allocate_dedicated(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plDeviceMetal* ptMetalDevice =ptData->ptDevice->_pInternalData;

    plDeviceAllocationBlock tBlock = {
        .ulAddress = 0,
        .ulSize    = ulSize
    };

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = uTypeFilter;
    ptHeapDescriptor.size        = tBlock.ulSize;
    ptHeapDescriptor.type        = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;

    tBlock.ulAddress = (uint64_t)[ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    
    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = tBlock.ulAddress,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptInst      = ptInst
    };

    uint32_t uBlockIndex = pl_sb_size(ptData->sbtBlocks);
    if(pl_sb_size(ptData->sbtFreeBlockIndices) > 0)
        uBlockIndex = pl_sb_pop(ptData->sbtFreeBlockIndices);
    else
        pl_sb_add(ptData->sbtBlocks);

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulTotalSize  = ulSize,
        .ulUsedSize   = ulSize,
        .ulBlockIndex = uBlockIndex
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    pl_sb_push(ptData->sbtNodes, tRange);
    ptData->sbtBlocks[uBlockIndex] = tBlock;
    [ptHeapDescriptor release];
    return tAllocation;
}

static void
pl_free_dedicated(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    uint32_t uBlockIndex = 0;
    uint32_t uNodeIndex = 0;
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];

        if(ptBlock->ulAddress == ptAllocation->uHandle)
        {
            uNodeIndex = i;
            uBlockIndex = (uint32_t)ptNode->ulBlockIndex;
            ptBlock->ulSize = 0;
            break;
        }
    }
    pl_sb_del_swap(ptData->sbtNodes, uNodeIndex);
    pl_sb_push(ptData->sbtFreeBlockIndices, uBlockIndex);

    id<MTLHeap> tHeap = (id<MTLHeap>)ptAllocation->uHandle;
    [tHeap setPurgeableState:MTLPurgeableStateEmpty];
    [tHeap release];
    tHeap = nil;

    ptAllocation->pHostMapped  = NULL;
    ptAllocation->uHandle      = 0;
    ptAllocation->ulOffset     = 0;
    ptAllocation->ulSize       = 0;
}

static plDeviceMemoryAllocation
pl_allocate_buddy(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plDeviceMetal* ptMetalDevice =ptData->ptDevice->_pInternalData;

    plDeviceMemoryAllocation tAllocation = pl__allocate_buddy(ptInst, uTypeFilter, ulSize, ulAlignment, pcName, 0);
    
    if(tAllocation.uHandle == 0)
    {
        plDeviceAllocationBlock* ptBlock = &pl_sb_top(ptData->sbtBlocks);
        MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
        ptHeapDescriptor.storageMode = uTypeFilter;
        ptHeapDescriptor.size        = PL_DEVICE_ALLOCATION_BLOCK_SIZE;
        ptHeapDescriptor.type        = MTLHeapTypePlacement;
        ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
        ptBlock->ulAddress = (uint64_t)[ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
        tAllocation.uHandle = (uint64_t)ptBlock->ulAddress;
    }

    return tAllocation;
}

static plDeviceMemoryAllocation
pl_allocate_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;
    plDeviceMetal* ptMetalDevice =ptData->ptDevice->_pInternalData;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = 0,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptInst      = ptInst
    };

    // check for existing block
    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptNode->ulBlockIndex];
        if(ptNode->ulUsedSize == 0 && ptNode->ulTotalSize >= ulSize)
        {
            ptNode->ulUsedSize = ulSize;
            pl_sprintf(ptNode->acName, "%s", pcName);
            tAllocation.pHostMapped = ptBlock->pHostMapped;
            tAllocation.uHandle = ptBlock->ulAddress;
            tAllocation.ulOffset = 0;
            tAllocation.ulSize = ptBlock->ulSize;
            return tAllocation;
        }
    }

    plDeviceAllocationBlock tBlock = {
        .ulAddress = 0,
        .ulSize    = pl_maxu((uint32_t)ulSize, PL_DEVICE_ALLOCATION_BLOCK_SIZE)
    };

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulUsedSize   = ulSize,
        .ulTotalSize  = tBlock.ulSize,
        .ulBlockIndex = pl_sb_size(ptData->sbtBlocks)
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = MTLStorageModeShared;
    ptHeapDescriptor.size = tBlock.ulSize;
    ptHeapDescriptor.type = MTLHeapTypePlacement;

    tBlock.ulAddress = (uint64_t)[ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    tAllocation.uHandle = tBlock.ulAddress;

    pl_sb_push(ptData->sbtNodes, tRange);
    pl_sb_push(ptData->sbtBlocks, tBlock);
    return tAllocation;
}

static void
pl_free_staging_uncached(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plDeviceAllocatorData* ptData = (plDeviceAllocatorData*)ptInst;

    for(uint32_t i = 0; i < pl_sb_size(ptData->sbtBlocks); i++)
    {
        plDeviceAllocationRange* ptRange = &ptData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptData->sbtBlocks[ptRange->ulBlockIndex];

        // find block
        if(ptBlock->ulAddress == ptAllocation->uHandle)
        {
            ptRange->ulUsedSize = 0;
            memset(ptRange->acName, 0, PL_MAX_NAME_LENGTH);
            strncpy(ptRange->acName, "not used", PL_MAX_NAME_LENGTH);
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static const plGraphicsI*
pl_load_graphics_api(void)
{
    static const plGraphicsI tApi = {
        .initialize                       = pl_initialize_graphics,
        .resize                           = pl_resize,
        .setup_ui                         = pl_setup_ui,
        .begin_frame                      = pl_begin_frame,
        .end_frame                        = pl_end_gfx_frame,
        .begin_recording                  = pl_begin_recording,
        .end_recording                    = pl_end_recording,
        .begin_main_pass                  = pl_begin_main_pass,
        .end_main_pass                    = pl_end_main_pass,
        .begin_pass                       = pl_begin_pass,
        .end_pass                         = pl_end_pass,
        .draw_areas                       = pl_draw_areas,
        .draw_lists                       = pl_draw_lists,
        .cleanup                          = pl_cleanup,
        .create_font_atlas                = pl_create_metal_font_texture,
        .destroy_font_atlas               = pl_cleanup_metal_font_texture,
        .add_3d_triangle_filled           = pl__add_3d_triangle_filled,
        .add_3d_line                      = pl__add_3d_line,
        .add_3d_point                     = pl__add_3d_point,
        .add_3d_transform                 = pl__add_3d_transform,
        .add_3d_frustum                   = pl__add_3d_frustum,
        .add_3d_centered_box              = pl__add_3d_centered_box,
        .add_3d_bezier_quad               = pl__add_3d_bezier_quad,
        .add_3d_bezier_cubic              = pl__add_3d_bezier_cubic,
        .register_3d_drawlist             = pl__register_3d_drawlist,
        .submit_3d_drawlist               = pl__submit_3d_drawlist,
        .get_ui_texture_handle            = pl_get_ui_texture_handle,
    };
    return &tApi;
}

static const plDeviceI*
pl_load_device_api(void)
{
    static const plDeviceI tApi = {
        .create_buffer                    = pl_create_buffer,
        .create_texture_view              = pl_create_texture_view,
        .create_texture                   = pl_create_texture,
        .create_shader                    = pl_create_shader,
        .create_render_pass_layout        = pl_create_render_pass_layout,
        .create_render_pass               = pl_create_render_pass,
        .create_frame_buffer              = pl_create_frame_buffer,
        .create_bind_group                = pl_create_bind_group,
        .update_bind_group                = pl_update_bind_group,
        .update_texture                   = pl_update_texture,
        .allocate_dynamic_data            = pl_allocate_dynamic_data,
        .submit_buffer_for_deletion       = pl_submit_buffer_for_deletion,
        .submit_texture_for_deletion      = pl_submit_texture_for_deletion,
        .submit_texture_view_for_deletion = pl_submit_texture_view_for_deletion,
        .get_buffer                       = pl__get_buffer,
        .get_texture                      = pl__get_texture,
        .get_texture_view                 = pl__get_texture_view,
        .get_bind_group                   = pl__get_bind_group,
        .get_shader                       = pl__get_shader,
        .submit_frame_buffer_for_deletion = pl_submit_frame_buffer_for_deletion
    };
    return &tApi;
}

PL_EXPORT void
pl_load_graphics_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
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
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DRAW_STREAM), pl_load_drawstream_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_GRAPHICS, pl_load_graphics_api());
        ptApiRegistry->add(PL_API_DEVICE, pl_load_device_api());
        ptApiRegistry->add(PL_API_DRAW_STREAM, pl_load_drawstream_api());
    }
}

PL_EXPORT void
pl_unload_graphics_ext(plApiRegistryApiI* ptApiRegistry)
{

}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "pl_ui_metal.m"