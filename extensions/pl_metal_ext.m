/*
   pl_metal_ext.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
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
#include "pl_memory.h"
#include "pl_graphics_ext.c"

// pilotlight ui
#include "pl_ui.h"
#include "pl_ui_metal.h"

// metal stuff
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_ARGUMENT_BUFFER_HEAP_SIZE
    #define PL_ARGUMENT_BUFFER_HEAP_SIZE 134217728
#endif

#ifndef PL_DYNAMIC_ARGUMENT_BUFFER_SIZE
    #define PL_DYNAMIC_ARGUMENT_BUFFER_SIZE 16777216
#endif

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

const plFileApiI*       gptFile = NULL;
const plOsServicesApiI* gptOS   = NULL;

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

typedef struct _plMetalSwapchain
{
    int unused;
} plMetalSwapchain;

typedef struct _plMetalRenderPassLayout
{
    int unused;
} plMetalRenderPassLayout;

typedef struct _plMetalRenderPass
{
    MTLRenderPassDescriptor* ptRenderPassDescriptor;
    plRenderPassAttachments  atFrameBuffers[6];
} plMetalRenderPass;

typedef struct _plMetalBuffer
{
    id<MTLBuffer> tBuffer;
    id<MTLHeap>   tHeap;
} plMetalBuffer;

typedef struct _plFrameContext
{
    dispatch_semaphore_t tFrameBoundarySemaphore;

    // temporary bind group stuff
    uint32_t       uCurrentArgumentBuffer;
    plMetalBuffer* sbtArgumentBuffers;
    size_t         szCurrentArgumentOffset;

    // dynamic buffer stuff
    uint32_t              uCurrentBufferIndex;
    plMetalDynamicBuffer* sbtDynamicBuffers;

    id<MTLFence> tFence;
} plFrameContext;

typedef struct _plMetalTexture
{
    id<MTLTexture> tTexture;
    id<MTLHeap>    tHeap;
} plMetalTexture;

typedef struct _plMetalSampler
{
    id<MTLSamplerState> tSampler;
} plMetalSampler;

typedef struct _plMetalTimelineSemaphore
{
    id<MTLEvent>       tEvent;
    id<MTLSharedEvent> tSharedEvent;
} plMetalTimelineSemaphore;

typedef struct _plMetalBindGroup
{
    id<MTLBuffer> tShaderArgumentBuffer;
    plBindGroupLayout tLayout;
    uint32_t uOffset;
} plMetalBindGroup;

typedef struct _plMetalShader
{
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;
    MTLCullMode                tCullMode;
    MTLTriangleFillMode        tFillMode;
    id<MTLLibrary>             library;
} plMetalShader;

typedef struct _plMetalComputeShader
{
    id<MTLComputePipelineState> tPipelineState;
    id<MTLLibrary> library;
} plMetalComputeShader;

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
    plTempAllocator     tTempAllocator;
    id<MTLCommandQueue> tCmdQueue;
    CAMetalLayer*       pMetalLayer;
    
    plFrameContext*           sbFrames;
    plMetalTexture*           sbtTexturesHot;
    plMetalSampler*           sbtSamplersHot;
    plMetalBindGroup*         sbtBindGroupsHot;
    plMetalBuffer*            sbtBuffersHot;
    plMetalShader*            sbtShadersHot;
    plMetalComputeShader*     sbtComputeShadersHot;
    plMetalRenderPass*        sbtRenderPassesHot;
    plMetalRenderPassLayout*  sbtRenderPassLayoutsHot;
    plMetalTimelineSemaphore* sbtSemaphoresHot;
    
    // drawing
    plMetalPipelineEntry*           sbtPipelineEntries;
    id<MTLFunction>                 tSolidVertexFunction;
    id<MTLFunction>                 tLineVertexFunction;
    id<MTLFunction>                 tFragmentFunction;
    NSMutableArray<plTrackedMetalBuffer*>* bufferCache;
    double                          lastBufferCachePurge;

    // per frame
    id<CAMetalDrawable>         tCurrentDrawable;
} plGraphicsMetal;

typedef struct _plDeviceMetal
{
    id<MTLDevice> tDevice;
    id<MTLHeap> tDescriptorHeap;
    uint64_t    ulDescriptorHeapOffset;
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
static MTLDataType            pl__metal_data_type  (plDataType tType);
static MTLRenderStages        pl__metal_stage_flags(plStageFlags tFlags);

static void                  pl__garbage_collect(plGraphics* ptGraphics);
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

static void
pl_create_main_render_pass_layout(plDevice* ptDevice)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassLayoutFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassLayoutFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_add(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_push(ptGraphics->sbtRenderPassLayoutGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtRenderPassLayoutsHot);
    }

    plRenderPassLayoutHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassLayoutGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPassLayout tLayout = {
        .tDesc = {
            .atRenderTargets = {
                {
                    .tClearColor = {0.0f, 0.0f, 0.0f, 1.0f},
                    .tFormat = ptGraphics->tSwapchain.tFormat,
                }
            }
        }
    };

    ptMetalGraphics->sbtRenderPassLayoutsHot[uResourceIndex] = (plMetalRenderPassLayout){0};
    ptGraphics->sbtRenderPassLayoutsCold[uResourceIndex] = tLayout;
    ptGraphics->tMainRenderPassLayout = tHandle;
}

static plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDescription* ptDesc)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassLayoutFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassLayoutFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_add(ptGraphics->sbtRenderPassLayoutsCold);
        pl_sb_push(ptGraphics->sbtRenderPassLayoutGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtRenderPassLayoutsHot);
    }

    plRenderPassLayoutHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassLayoutGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPassLayout tLayout = {
        .tDesc = *ptDesc
    };

    // find attachment count & fill out references & descriptions
    uint32_t uAttachmentCount = 0;

    if(ptDesc->tDepthTargetFormat != PL_FORMAT_UNKNOWN)
        uAttachmentCount = 1;

    for(uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if(ptDesc->atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
            break;
        uAttachmentCount++;
    }
    tLayout._uAttachmentCount = uAttachmentCount;

    ptMetalGraphics->sbtRenderPassLayoutsHot[uResourceIndex] = (plMetalRenderPassLayout){0};
    ptGraphics->sbtRenderPassLayoutsCold[uResourceIndex] = tLayout;
    return tHandle;
}

static void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{

    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGfx = ptGraphics->_pInternalData;

    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[tHandle.uIndex];
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGfx->sbtRenderPassesHot[tHandle.uIndex];
    ptRenderPass->tDesc.tDimensions = tDimensions;

    ptMetalRenderPass->atFrameBuffers[0] = ptAttachments[0];
}

static void
pl_create_main_render_pass(plDevice* ptDevice)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassesCold);
        pl_sb_add(ptGraphics->sbtRenderPassesCold);
        pl_sb_push(ptGraphics->sbtRenderPassGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtRenderPassesHot);
    }

    plRenderPassHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPass tRenderPass = {
        .tDesc = {
            .tDimensions = {pl_get_io()->afMainViewportSize[0], pl_get_io()->afMainViewportSize[1]}
        },
        .bSwapchain = true
    };

    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptGraphics->tMainRenderPassLayout.uIndex];

    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[uResourceIndex];

    // render pass descriptor
    ptMetalRenderPass->ptRenderPassDescriptor = [MTLRenderPassDescriptor new];

    ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    ptMetalRenderPass->ptRenderPassDescriptor.depthAttachment.texture = nil;

    ptGraphics->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
    ptGraphics->tMainRenderPass = tHandle;
}

static plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDescription* ptDesc, const plRenderPassAttachments* ptAttachments)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtRenderPassFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtRenderPassFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtRenderPassesCold);
        pl_sb_add(ptGraphics->sbtRenderPassesCold);
        pl_sb_push(ptGraphics->sbtRenderPassGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtRenderPassesHot);
    }

    plRenderPassHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtRenderPassGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plRenderPass tRenderPass = {
        .tDesc = *ptDesc
    };

    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptDesc->tLayout.uIndex];

    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[uResourceIndex];

    // render pass descriptor
    ptMetalRenderPass->ptRenderPassDescriptor = [MTLRenderPassDescriptor new];

    if(ptLayout->tDesc.tDepthTargetFormat != PL_FORMAT_UNKNOWN)
    {
        ptMetalRenderPass->ptRenderPassDescriptor.depthAttachment.loadAction = pl__metal_load_op(ptDesc->tDepthTarget.tLoadOp);
        ptMetalRenderPass->ptRenderPassDescriptor.depthAttachment.storeAction = pl__metal_store_op(ptDesc->tDepthTarget.tStoreOp);
        ptMetalRenderPass->ptRenderPassDescriptor.depthAttachment.clearDepth = ptDesc->tDepthTarget.fClearZ;
    }

    for(uint32_t i = 0; i < 16; i++)
    {
        if(ptLayout->tDesc.atRenderTargets[i].tFormat == PL_FORMAT_UNKNOWN)
        {
            break;
        }

        ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[i].loadAction = pl__metal_load_op(ptDesc->atRenderTargets[i].tLoadOp);
        ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[i].storeAction = pl__metal_store_op(ptDesc->atRenderTargets[i].tStoreOp);
        ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(
            ptDesc->atRenderTargets[i].tClearColor.r,
            ptDesc->atRenderTargets[i].tClearColor.g,
            ptDesc->atRenderTargets[i].tClearColor.b,
            ptDesc->atRenderTargets[i].tClearColor.a
            );
    }

    ptMetalRenderPass->atFrameBuffers[0] = ptAttachments[0];

    ptGraphics->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
    return tHandle;
}

static void
pl_copy_buffer_to_texture(plBlitEncoder* ptEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plDevice*        ptDevice       = &ptEncoder->ptGraphics->tDevice;
    plDeviceMetal*   ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptEncoder->ptGraphics->_pInternalData;

    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    id<MTLBlitCommandEncoder> blitEncoder = (id<MTLBlitCommandEncoder>)ptEncoder->_pInternal;

    plMetalBuffer* ptBuffer = &ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex];
    plMetalTexture* ptTexture = &ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex];
    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {

        MTLOrigin tOrigin;
        tOrigin.x = ptRegions[i].iImageOffsetX;
        tOrigin.y = ptRegions[i].iImageOffsetY;
        tOrigin.z = ptRegions[i].iImageOffsetZ;

        MTLSize tSize;
        tSize.width  = ptRegions[i].tImageExtent.uWidth;
        tSize.height = ptRegions[i].tImageExtent.uHeight;
        tSize.depth  = ptRegions[i].tImageExtent.uDepth;

        NSUInteger uBytesPerRow = tSize.width * pl__format_stride(ptColdTexture->tDesc.tFormat);
        [blitEncoder copyFromBuffer:ptBuffer->tBuffer
            sourceOffset:ptRegions[i].szBufferOffset
            sourceBytesPerRow:uBytesPerRow 
            sourceBytesPerImage:0 
            sourceSize:tSize 
            toTexture:ptTexture->tTexture
            destinationSlice:ptRegions[i].uBaseArrayLayer
            destinationLevel:0 
            destinationOrigin:tOrigin];
    }
}

static void
pl_transfer_image_to_buffer(plBlitEncoder* ptEncoder, plTextureHandle tTexture, plBufferHandle tBuffer)
{
    plGraphicsMetal* ptMetalGraphics = ptEncoder->ptGraphics->_pInternalData;
    id<MTLBlitCommandEncoder> blitEncoder = (id<MTLBlitCommandEncoder>)ptEncoder->_pInternal;

    const plTexture* ptTexture = pl__get_texture(&ptEncoder->ptGraphics->tDevice, tTexture);
    const plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[tTexture.uIndex];
    const plMetalBuffer* ptMetalBuffer = &ptMetalGraphics->sbtBuffersHot[tBuffer.uIndex];

    MTLOrigin tOrigin;
    tOrigin.x = 0;
    tOrigin.y = 0;
    tOrigin.z = 0;
    MTLSize tSize;
    tSize.width = ptTexture->tDesc.tDimensions.x;
    tSize.height = ptTexture->tDesc.tDimensions.y;
    tSize.depth = ptTexture->tDesc.tDimensions.z;

    const uint32_t uFormatStride = pl__format_stride(ptTexture->tDesc.tFormat);

    [blitEncoder copyFromTexture:ptMetalTexture->tTexture
        sourceSlice:0
        sourceLevel:0
        sourceOrigin:tOrigin
        sourceSize:tSize
        toBuffer:ptMetalBuffer->tBuffer
        destinationOffset:0
        destinationBytesPerRow:ptTexture->tDesc.tDimensions.x * uFormatStride
        destinationBytesPerImage:0];
}

static void
pl_copy_buffer(plBlitEncoder* ptEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{
    plGraphicsMetal* ptMetalGraphics = ptEncoder->ptGraphics->_pInternalData;
    id<MTLBlitCommandEncoder> blitEncoder = (id<MTLBlitCommandEncoder>)ptEncoder->_pInternal;
    [blitEncoder copyFromBuffer:ptMetalGraphics->sbtBuffersHot[tSource.uIndex].tBuffer sourceOffset:uSourceOffset toBuffer:ptMetalGraphics->sbtBuffersHot[tDestination.uIndex].tBuffer destinationOffset:uDestinationOffset size:szSize];
}

static plSemaphoreHandle
pl_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{   
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;

    uint32_t uIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtSemaphoreFreeIndices) > 0)
        uIndex = pl_sb_pop(ptGraphics->sbtSemaphoreFreeIndices);
    else
    {
        uIndex = pl_sb_size(ptMetalGraphics->sbtSemaphoresHot);
        pl_sb_push(ptGraphics->sbtSemaphoreGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtSemaphoresHot);
    }

    plSemaphoreHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtSemaphoreGenerations[uIndex],
        .uIndex = uIndex
    };
    
    plMetalTimelineSemaphore tSemaphore = {0};
    if(bHostVisible)
    {
        tSemaphore.tSharedEvent = [ptMetalDevice->tDevice newSharedEvent];
    }
    else
    {
        tSemaphore.tEvent = [ptMetalDevice->tDevice newEvent];
    }
    ptMetalGraphics->sbtSemaphoresHot[uIndex] = tSemaphore;
    return tHandle;
}

static void
pl_signal_semaphore(plGraphics* ptGraphics, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    PL_ASSERT(ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent != nil);
    if(ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent)
    {
        ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent.signaledValue = ulValue;
    }
}

static void
pl_wait_semaphore(plGraphics* ptGraphics, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    PL_ASSERT(ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent != nil);
    if(ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent)
    {
        while(ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent.signaledValue != ulValue)
        {
            gptOS->sleep(1);
        }
    }
}

static uint64_t
pl_get_semaphore_value(plGraphics* ptGraphics, plSemaphoreHandle tHandle)
{
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    PL_ASSERT(ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent != nil);

    if(ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent)
    {
        return ptMetalGraphics->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent.signaledValue;
    }
    return 0;
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

        tBuffer.tMemoryAllocation.pHostMapped = tMetalBuffer.tBuffer.contents;
        tBuffer.tMemoryAllocation.ulOffset = 0;
        tBuffer.tMemoryAllocation.ulSize = ptDesc->uByteSize;
        tMetalBuffer.tHeap = (id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle;
        ptMetalGraphics->sbtBuffersHot[uBufferIndex] = tMetalBuffer;
    }
    else if(ptDesc->tMemory == PL_MEMORY_GPU)
    {

        plDeviceMemoryAllocatorI* ptAllocator = ptDesc->uByteSize > PL_DEVICE_BUDDY_BLOCK_SIZE ? &ptDevice->tLocalDedicatedAllocator : &ptDevice->tLocalBuddyAllocator;
        tBuffer.tMemoryAllocation = ptAllocator->allocate(ptAllocator->ptInst, MTLStorageModePrivate, ptDesc->uByteSize, 0, pcName);

        plMetalBuffer tMetalBuffer = {
            .tBuffer = [(id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle newBufferWithLength:ptDesc->uByteSize options:MTLResourceStorageModePrivate offset:tBuffer.tMemoryAllocation.ulOffset]
        };
        tMetalBuffer.tBuffer.label = [NSString stringWithUTF8String:ptDesc->acDebugName];

        tMetalBuffer.tHeap = (id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle;
        ptMetalGraphics->sbtBuffersHot[uBufferIndex] = tMetalBuffer;
    }
    else if(ptDesc->tMemory == PL_MEMORY_CPU)
    {
        tBuffer.tMemoryAllocation = ptDevice->tStagingCachedAllocator.allocate(ptDevice->tStagingCachedAllocator.ptInst, MTLStorageModePrivate, ptDesc->uByteSize, 0, pcName);

        plMetalBuffer tMetalBuffer = {
            .tBuffer = [(id<MTLHeap>)tBuffer.tMemoryAllocation.uHandle newBufferWithLength:ptDesc->uByteSize options:MTLResourceStorageModeShared offset:0]
        };
        tMetalBuffer.tBuffer.label = [NSString stringWithUTF8String:ptDesc->acDebugName];
        memset(tMetalBuffer.tBuffer.contents, 0, ptDesc->uByteSize);

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
pl_generate_mipmaps(plBlitEncoder* ptEncoder, plTextureHandle tTexture)
{
    plGraphicsMetal* ptMetalGraphics = ptEncoder->ptGraphics->_pInternalData;
    plTexture* ptTexture = pl__get_texture(&ptEncoder->ptGraphics->tDevice, tTexture);
    if(ptTexture->tDesc.uMips < 2)
        return;

    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    id<MTLBlitCommandEncoder> blitEncoder = (id<MTLBlitCommandEncoder>)ptEncoder->_pInternal;
    [blitEncoder generateMipmapsForTexture:ptMetalGraphics->sbtTexturesHot[tTexture.uIndex].tTexture];
}

static plTextureHandle
pl_create_texture(plDevice* ptDevice, plTextureDesc tDesc, const char* pcName)
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

    MTLTextureDescriptor* ptTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    ptTextureDescriptor.pixelFormat = pl__metal_format(tDesc.tFormat);
    ptTextureDescriptor.width = tDesc.tDimensions.x;
    ptTextureDescriptor.height = tDesc.tDimensions.y;
    ptTextureDescriptor.mipmapLevelCount = tDesc.uMips;
    ptTextureDescriptor.storageMode = MTLStorageModePrivate;
    ptTextureDescriptor.arrayLength = 1;
    ptTextureDescriptor.depth = tDesc.tDimensions.z;
    ptTextureDescriptor.sampleCount = 1;

    if(tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
        ptTextureDescriptor.usage |= MTLTextureUsageShaderRead;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;

    // if(tDesc.tUsage & PL_TEXTURE_USAGE_TRANSIENT_ATTACHMENT)
    //     ptTextureDescriptor.storageMode = MTLStorageModeMemoryless;

    if(tDesc.tType == PL_TEXTURE_TYPE_2D)
        ptTextureDescriptor.textureType = MTLTextureType2D;
    else if(tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        ptTextureDescriptor.textureType = MTLTextureTypeCube;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    MTLSizeAndAlign tSizeAndAlign = [ptMetalDevice->tDevice heapTextureSizeAndAlignWithDescriptor:ptTextureDescriptor];
    plDeviceMemoryAllocatorI* ptAllocator = tSizeAndAlign.size > PL_DEVICE_BUDDY_BLOCK_SIZE ? &ptGraphics->tDevice.tLocalDedicatedAllocator : &ptGraphics->tDevice.tLocalBuddyAllocator;
    tTexture.tMemoryAllocation = ptAllocator->allocate(ptAllocator->ptInst, ptTextureDescriptor.storageMode, tSizeAndAlign.size, tSizeAndAlign.align, pcName);

    plMetalTexture tMetalTexture = {
        .tTexture = [(id<MTLHeap>)tTexture.tMemoryAllocation.uHandle newTextureWithDescriptor:ptTextureDescriptor offset:tTexture.tMemoryAllocation.ulOffset],
        .tHeap = (id<MTLHeap>)tTexture.tMemoryAllocation.uHandle
    };
    tMetalTexture.tTexture.label = [NSString stringWithUTF8String:pcName];

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
    samplerDesc.mipFilter = MTLSamplerMipFilterNearest;
    samplerDesc.normalizedCoordinates = YES;
    samplerDesc.supportArgumentBuffers = YES;
    samplerDesc.sAddressMode = pl__metal_wrap(ptSampler->tHorizontalWrap);
    samplerDesc.tAddressMode = pl__metal_wrap(ptSampler->tVerticalWrap);
    samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack;
    samplerDesc.compareFunction = pl__metal_compare(ptSampler->tCompare);
    samplerDesc.lodMinClamp = ptSampler->fMinMip;
    samplerDesc.lodMaxClamp = ptSampler->fMaxMip;
    samplerDesc.label = [NSString stringWithUTF8String:pcName];

    if(ptSampler->fMaxMip == PL_MAX_MIPS)
        samplerDesc.lodMaxClamp = tTextureView.tTextureViewDesc.uMips;

    plMetalSampler tMetalSampler = {
        .tSampler = [ptMetalDevice->tDevice newSamplerStateWithDescriptor:samplerDesc]
    };

    ptMetalGraphics->sbtSamplersHot[uTextureViewIndex] = tMetalSampler;
    ptGraphics->sbtTextureViewsCold[uTextureViewIndex] = tTextureView;
    return tHandle;
}

static plBindGroupHandle
pl_get_temporary_bind_group(plDevice* ptDevice, plBindGroupLayout* ptLayout)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    uint32_t uBindGroupIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtBindGroupFreeIndices) > 0)
        uBindGroupIndex = pl_sb_pop(ptGraphics->sbtBindGroupFreeIndices);
    else
    {
        uBindGroupIndex = pl_sb_size(ptGraphics->sbtBindGroupsCold);
        pl_sb_add(ptGraphics->sbtBindGroupsCold);
        pl_sb_push(ptGraphics->sbtBindGroupGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtBindGroupsHot);
    }

    plBindGroupHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtBindGroupGenerations[uBindGroupIndex],
        .uIndex = uBindGroupIndex
    };

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    NSUInteger argumentBufferLength = sizeof(MTLResourceID) * ptLayout->uTextureCount * 2 + sizeof(void*) * ptLayout->uBufferCount;

    if(argumentBufferLength + ptFrame->szCurrentArgumentOffset > PL_DYNAMIC_ARGUMENT_BUFFER_SIZE)
    {
        ptFrame->uCurrentArgumentBuffer++;
        if(ptFrame->uCurrentArgumentBuffer >= pl_sb_size(ptFrame->sbtArgumentBuffers))
        {
            plMetalBuffer tArgumentBuffer = {
                .tBuffer = [ptMetalDevice->tDescriptorHeap newBufferWithLength:PL_DYNAMIC_ARGUMENT_BUFFER_SIZE options:MTLResourceStorageModeShared offset:ptMetalDevice->ulDescriptorHeapOffset]
            };
            ptMetalDevice->ulDescriptorHeapOffset += argumentBufferLength;
            ptMetalDevice->ulDescriptorHeapOffset = PL__ALIGN_UP(ptMetalDevice->ulDescriptorHeapOffset, 256);

            pl_sb_push(ptFrame->sbtArgumentBuffers, tArgumentBuffer);
        }
         ptFrame->szCurrentArgumentOffset = 0;
    }

    plMetalBindGroup tMetalBindGroup = {
        .tShaderArgumentBuffer = ptFrame->sbtArgumentBuffers[ptFrame->uCurrentArgumentBuffer].tBuffer,
        .uOffset = ptFrame->szCurrentArgumentOffset
    };
    ptFrame->szCurrentArgumentOffset += argumentBufferLength;

    [tMetalBindGroup.tShaderArgumentBuffer retain];
    tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:"temp bind group"];

    ptMetalGraphics->sbtBindGroupsHot[uBindGroupIndex] = tMetalBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    pl_queue_bind_group_for_deletion(ptDevice, tHandle);
    return tHandle;
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, plBindGroupLayout* ptLayout)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uBindGroupIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtBindGroupFreeIndices) > 0)
        uBindGroupIndex = pl_sb_pop(ptGraphics->sbtBindGroupFreeIndices);
    else
    {
        uBindGroupIndex = pl_sb_size(ptGraphics->sbtBindGroupsCold);
        pl_sb_add(ptGraphics->sbtBindGroupsCold);
        pl_sb_push(ptGraphics->sbtBindGroupGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtBindGroupsHot);
    }

    plBindGroupHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtBindGroupGenerations[uBindGroupIndex],
        .uIndex = uBindGroupIndex
    };

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    NSUInteger argumentBufferLength = sizeof(MTLResourceID) * ptLayout->uTextureCount * 2 + sizeof(void*) * ptLayout->uBufferCount;

    plMetalBindGroup tMetalBindGroup = {
        .tShaderArgumentBuffer = [ptMetalDevice->tDescriptorHeap newBufferWithLength:argumentBufferLength options:MTLResourceStorageModeShared offset:ptMetalDevice->ulDescriptorHeapOffset]
    };
    tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:"bind group"];
    ptMetalDevice->ulDescriptorHeapOffset += argumentBufferLength;
    ptMetalDevice->ulDescriptorHeapOffset = PL__ALIGN_UP(ptMetalDevice->ulDescriptorHeapOffset, 256);

    ptMetalGraphics->sbtBindGroupsHot[uBindGroupIndex] = tMetalBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
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

    const char* pcDescriptorStart = ptMetalBindGroup->tShaderArgumentBuffer.contents;
    
    // start of buffers
    float** ptBufferResources = (float**)&pcDescriptorStart[ptMetalBindGroup->uOffset];
    for(uint32_t i = 0; i < uBufferCount; i++)
    {
        plMetalBuffer* ptMetalBuffer = &ptMetalGraphics->sbtBuffersHot[atBuffers[i].uIndex];
        ptBufferResources[i] = (float*)ptMetalBuffer->tBuffer.gpuAddress;
        ptBindGroup->tLayout.aBuffers[i].tBuffer = atBuffers[i];
    }

    // start of textures
    char* pcStartOfBuffers = (char*)&pcDescriptorStart[ptMetalBindGroup->uOffset];

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
            ptDynamicBuffer->tBuffer.label = [NSString stringWithUTF8String:"buddy allocator"];
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

static plComputeShaderHandle
pl_get_compute_shader_variant(plDevice* ptDevice, plComputeShaderHandle tHandle, const plComputeShaderVariant* ptVariant)
{
    plGraphics*       ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal*   ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    plComputeShader* ptShader = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < ptShader->tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDescription.atConstants[i];
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
    }

    const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, uTotalConstantSize, 0);
    const uint64_t ulIndex = pl_hm_lookup(&ptShader->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptShader->_sbtVariantHandles[ulIndex];

    uint32_t uNewResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtComputeShaderFreeIndices) > 0)
        uNewResourceIndex = pl_sb_pop(ptGraphics->sbtComputeShaderFreeIndices);
    else
    {
        uNewResourceIndex = pl_sb_size(ptGraphics->sbtComputeShadersCold);
        pl_sb_add(ptGraphics->sbtComputeShadersCold);
        pl_sb_push(ptGraphics->sbtComputeShaderGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtComputeShadersHot);
    }
    ptShader = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];
    plMetalComputeShader* ptMetalShader = &ptMetalGraphics->sbtComputeShadersHot[uNewResourceIndex];


    plComputeShaderHandle tVariantHandle = {
        .uGeneration = ++ptGraphics->sbtComputeShaderGenerations[uNewResourceIndex],
        .uIndex = uNewResourceIndex
    };

    pl_hm_insert(&ptShader->tVariantHashmap, ulVariantHash, pl_sb_size(ptShader->_sbtVariantHandles));
    pl_sb_push(ptShader->_sbtVariantHandles, tVariantHandle);

    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptVariant->pTempConstantData;
    for(uint32_t i = 0; i < ptShader->tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDescription.atConstants[i];
        [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
    }

    NSError* error = nil;
    id<MTLFunction> computeFunction = [ptMetalShader->library newFunctionWithName:@"kernel_main" constantValues:ptConstantValues error:&error];

    if (computeFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
    }

    const plMetalComputeShader tMetalShader = {
        .tPipelineState = [ptMetalDevice->tDevice newComputePipelineStateWithFunction:computeFunction error:&error]
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    ptMetalGraphics->sbtComputeShadersHot[uNewResourceIndex] = tMetalShader;
    ptGraphics->sbtComputeShadersCold[uNewResourceIndex] = *ptShader;
    ptGraphics->sbtComputeShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
    memset(&ptGraphics->sbtComputeShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));
    return tVariantHandle;
}

static plComputeShaderHandle
pl_create_compute_shader(plDevice* ptDevice, const plComputeShaderDescription* ptDescription)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtComputeShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtComputeShaderFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtComputeShadersCold);
        pl_sb_add(ptGraphics->sbtComputeShadersCold);
        pl_sb_push(ptGraphics->sbtComputeShaderGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtComputeShadersHot);
    }

    plComputeShaderHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtComputeShaderGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plComputeShader tShader = {
        .tDescription = *ptDescription
    };

    plMetalComputeShader* ptMetalShader = &ptMetalGraphics->sbtComputeShadersHot[uResourceIndex];

    if(ptDescription->pcShaderEntryFunc == NULL)
        tShader.tDescription.pcShaderEntryFunc = "kernel_main";

    // read in shader source code
    unsigned uShaderFileSize = 0;
    gptFile->read(tShader.tDescription.pcShader, &uShaderFileSize, NULL, "rb");
    char* pcFileData = pl_temp_allocator_alloc(&ptMetalGraphics->tTempAllocator, uShaderFileSize + 1);
    memset(pcFileData, 0, uShaderFileSize + 1);
    gptFile->read(tShader.tDescription.pcShader, &uShaderFileSize, pcFileData, "rb");

    // compile shader source
    NSError* error = nil;
    NSString* shaderSource = [NSString stringWithUTF8String:pcFileData];
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];
    ptMetalShader->library = [ptMetalDevice->tDevice  newLibraryWithSource:shaderSource options:ptCompileOptions error:&error];
    if (ptMetalShader->library == nil)
    {
        NSLog(@"Error: failed to create Metal library: %@", error);
    }
    pl_temp_allocator_reset(&ptMetalGraphics->tTempAllocator);

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
    }

    const plComputeShaderVariant tMainShaderVariant = {.pTempConstantData = tShader.tDescription.pTempConstantData};

    plComputeShaderVariant *ptVariants = pl_temp_allocator_alloc(&ptMetalGraphics->tTempAllocator, sizeof(plComputeShaderVariant) * (tShader.tDescription.uVariantCount + 1));
    ptVariants[0] = tMainShaderVariant;
    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount; i++)
    {
        ptVariants[i + 1] = tShader.tDescription.ptVariants[i];
    }

    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount + 1; i++)
    {
       const plComputeShaderVariant *ptVariant = &ptVariants[i];

        uint32_t uNewResourceIndex = UINT32_MAX;

        if(i == 0)
            uNewResourceIndex = uResourceIndex;
        else
        {
            if(pl_sb_size(ptGraphics->sbtComputeShaderFreeIndices) > 0)
                uNewResourceIndex = pl_sb_pop(ptGraphics->sbtComputeShaderFreeIndices);
            else
            {
                uNewResourceIndex = pl_sb_size(ptGraphics->sbtComputeShadersCold);
                pl_sb_add(ptGraphics->sbtComputeShadersCold);
                pl_sb_push(ptGraphics->sbtComputeShaderGenerations, UINT32_MAX);
                pl_sb_add(ptMetalGraphics->sbtComputeShadersHot);
                ptMetalShader = &ptMetalGraphics->sbtComputeShadersHot[uResourceIndex];
            }
        }

        plComputeShaderHandle tVariantHandle = {
            .uGeneration = ++ptGraphics->sbtComputeShaderGenerations[uNewResourceIndex],
            .uIndex = uNewResourceIndex
        };

        const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, uTotalConstantSize, 0);
        pl_hm_insert(&tShader.tVariantHashmap, ulVariantHash, pl_sb_size(tShader._sbtVariantHandles));
        pl_sb_push(tShader._sbtVariantHandles, tVariantHandle);

        MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

        const char* pcConstantData = ptVariant->pTempConstantData;
        for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
        {
            const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
            [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
        }

        id<MTLFunction> computeFunction = [ptMetalShader->library newFunctionWithName:@"kernel_main" constantValues:ptConstantValues error:&error];

        if (computeFunction == nil)
        {
            NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        }

        const plMetalComputeShader tMetalShader = {
            .tPipelineState = [ptMetalDevice->tDevice newComputePipelineStateWithFunction:computeFunction error:&error]
        };

        if (error != nil)
            NSLog(@"Error: failed to create Metal pipeline state: %@", error);

        ptGraphics->sbtComputeShadersCold[uNewResourceIndex] = tShader;
        if(i == 0)
        {
            ptMetalShader->tPipelineState = tMetalShader.tPipelineState;
        }
        else
        {
            ptMetalGraphics->sbtComputeShadersHot[uNewResourceIndex] = tMetalShader;
            ptGraphics->sbtComputeShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
            memset(&ptGraphics->sbtComputeShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));
        }
    }
    ptGraphics->sbtComputeShadersCold[uResourceIndex] = tShader;
    return tHandle;
}

static plShaderHandle
pl_get_shader_variant(plDevice* ptDevice, plShaderHandle tHandle, const plShaderVariant* ptVariant)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    plShader* ptShader = &ptGraphics->sbtShadersCold[tHandle.uIndex];

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < ptShader->tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDescription.atConstants[i];
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
    }

    const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, uTotalConstantSize, ptVariant->tGraphicsState.ulValue);
    const uint64_t ulIndex = pl_hm_lookup(&ptShader->tVariantHashmap, ulVariantHash);

    if(ulIndex != UINT64_MAX)
        return ptShader->_sbtVariantHandles[ulIndex];;

    uint32_t uNewResourceIndex = UINT32_MAX;

    if(pl_sb_size(ptGraphics->sbtShaderFreeIndices) > 0)
        uNewResourceIndex = pl_sb_pop(ptGraphics->sbtShaderFreeIndices);
    else
    {
        uNewResourceIndex = pl_sb_size(ptGraphics->sbtShadersCold);
        pl_sb_add(ptGraphics->sbtShadersCold);
        pl_sb_push(ptGraphics->sbtShaderGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtShadersHot);
        ptShader = &ptGraphics->sbtShadersCold[tHandle.uIndex];
    }

    plMetalShader* ptMetalShader = &ptMetalGraphics->sbtShadersHot[tHandle.uIndex];
    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptVariant->pTempConstantData;
    for(uint32_t i = 0; i < ptShader->tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDescription.atConstants[i];
        [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
    }

    NSError* error = nil;
    id<MTLFunction> vertexFunction = [ptMetalShader->library newFunctionWithName:@"vertex_main" constantValues:ptConstantValues error:&error];
    id<MTLFunction> fragmentFunction = [ptMetalShader->library newFunctionWithName:@"fragment_main" constantValues:ptConstantValues error:&error];

    if (vertexFunction == nil || fragmentFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
    }

    MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
    depthDescriptor.depthCompareFunction = pl__metal_compare((plCompareMode)ptVariant->tGraphicsState.ulDepthMode);
    depthDescriptor.depthWriteEnabled = ptVariant->tGraphicsState.ulDepthWriteEnabled ? YES : NO;

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
    const plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptShader->tDescription.tRenderPassLayout.uIndex];

    uint32_t uColorAttachmentCount = ptLayout->_uAttachmentCount;
    if(ptLayout->tDesc.tDepthTargetFormat != PL_FORMAT_UNKNOWN)
    {
        pipelineDescriptor.depthAttachmentPixelFormat = pl__metal_format(ptLayout->tDesc.tDepthTargetFormat);
        uColorAttachmentCount--;
    }
    for(uint32_t j = 0; j < uColorAttachmentCount; j++)
    {
        if(j == 0)
        {
            pipelineDescriptor.colorAttachments[j].pixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[j].tFormat);
            pipelineDescriptor.colorAttachments[j].blendingEnabled = YES;
            pipelineDescriptor.colorAttachments[j].rgbBlendOperation = MTLBlendOperationAdd;
            pipelineDescriptor.colorAttachments[j].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            pipelineDescriptor.colorAttachments[j].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipelineDescriptor.colorAttachments[j].alphaBlendOperation = MTLBlendOperationAdd;
            pipelineDescriptor.colorAttachments[j].sourceAlphaBlendFactor = MTLBlendFactorOne;
            pipelineDescriptor.colorAttachments[j].destinationAlphaBlendFactor = MTLBlendFactorZero;
        }
        else
        {
            pipelineDescriptor.colorAttachments[j].pixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[j].tFormat);
            pipelineDescriptor.colorAttachments[j].blendingEnabled = NO;
        }
    }

    const plMetalShader tMetalShader = {
        .tDepthStencilState   = [ptMetalDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor],
        .tRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error],
        .tCullMode            = pl__metal_cull(ptVariant->tGraphicsState.ulCullMode),
        .tFillMode            = ptVariant->tGraphicsState.ulWireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    plShaderHandle tVariantHandle = {
        .uGeneration = ++ptGraphics->sbtShaderGenerations[uNewResourceIndex],
        .uIndex = uNewResourceIndex
    };
    
    pl_hm_insert(&ptShader->tVariantHashmap, ulVariantHash, pl_sb_size(ptShader->_sbtVariantHandles));
    pl_sb_push(ptShader->_sbtVariantHandles, tVariantHandle);

    ptGraphics->sbtShadersCold[uNewResourceIndex] = *ptShader;
    ptMetalGraphics->sbtShadersHot[uNewResourceIndex] = tMetalShader;
    ptGraphics->sbtShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
    memset(&ptGraphics->sbtShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));
    return tVariantHandle;
}

static plShaderHandle
pl_create_shader(plDevice* ptDevice, const plShaderDescription* ptDescription)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtShaderFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtShadersCold);
        pl_sb_add(ptGraphics->sbtShadersCold);
        pl_sb_push(ptGraphics->sbtShaderGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtShadersHot);
    }

    plShaderHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtShaderGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

    plShader tShader = {
        .tDescription = *ptDescription
    };

    plMetalShader* ptMetalShader = &ptMetalGraphics->sbtShadersHot[uResourceIndex];

    if(ptDescription->pcPixelShaderEntryFunc == NULL)
        tShader.tDescription.pcPixelShaderEntryFunc = "fragment_main";

    if(ptDescription->pcVertexShaderEntryFunc == NULL)
        tShader.tDescription.pcVertexShaderEntryFunc = "vertex_main";

    // vertex layout
    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3; // position
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stride = sizeof(float) * 3;

    // read in shader source code
    unsigned uShaderFileSize = 0;
    gptFile->read(tShader.tDescription.pcVertexShader, &uShaderFileSize, NULL, "rb");
    char* pcFileData = pl_temp_allocator_alloc(&ptMetalGraphics->tTempAllocator, uShaderFileSize + 1);
    memset(pcFileData, 0, uShaderFileSize + 1);
    gptFile->read(tShader.tDescription.pcVertexShader, &uShaderFileSize, pcFileData, "rb");

    // prepare preprocessor defines
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];

    // compile shader source
    NSError* error = nil;
    NSString* shaderSource = [NSString stringWithUTF8String:pcFileData];
    ptMetalShader->library = [ptMetalDevice->tDevice  newLibraryWithSource:shaderSource options:ptCompileOptions error:&error];
    if (ptMetalShader->library == nil)
    {
        NSLog(@"Error: failed to create Metal library: %@", error);
    }

    pl_temp_allocator_reset(&ptMetalGraphics->tTempAllocator);

    // renderpass stuff
    const plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[tShader.tDescription.tRenderPassLayout.uIndex];

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
    }

    const plRenderPassLayout* ptRenderPassLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptDescription->tRenderPassLayout.uIndex];

    const plShaderVariant tMainShaderVariant = {.pTempConstantData = tShader.tDescription.pTempConstantData, .tGraphicsState = tShader.tDescription.tGraphicsState};
    plShaderVariant *ptVariants = pl_temp_allocator_alloc(&ptMetalGraphics->tTempAllocator, sizeof(plShaderVariant) * (tShader.tDescription.uVariantCount + 1));
    ptVariants[0] = tMainShaderVariant;
    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount; i++)
    {
        ptVariants[i + 1] = tShader.tDescription.ptVariants[i];
    }
    for(uint32_t i = 0; i < tShader.tDescription.uVariantCount + 1; i++)
    {
        const plShaderVariant *ptVariant = &ptVariants[i];

        uint32_t uNewResourceIndex = UINT32_MAX;

        if(i == 0)
            uNewResourceIndex = uResourceIndex;
        else
        {
            if(pl_sb_size(ptGraphics->sbtShaderFreeIndices) > 0)
                uNewResourceIndex = pl_sb_pop(ptGraphics->sbtShaderFreeIndices);
            else
            {
                uNewResourceIndex = pl_sb_size(ptGraphics->sbtShadersCold);
                pl_sb_add(ptGraphics->sbtShadersCold);
                pl_sb_push(ptGraphics->sbtShaderGenerations, UINT32_MAX);
                pl_sb_add(ptMetalGraphics->sbtShadersHot);
                ptMetalShader = &ptMetalGraphics->sbtShadersHot[uResourceIndex];
            }
        }

        MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

        const char* pcConstantData = ptVariant->pTempConstantData;
        for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
        {
            const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
            [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
        }

        id<MTLFunction> vertexFunction = [ptMetalShader->library newFunctionWithName:@"vertex_main" constantValues:ptConstantValues error:&error];
        id<MTLFunction> fragmentFunction = [ptMetalShader->library newFunctionWithName:@"fragment_main" constantValues:ptConstantValues error:&error];

        if (vertexFunction == nil || fragmentFunction == nil)
        {
            NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        }

        MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
        depthDescriptor.depthCompareFunction = pl__metal_compare((plCompareMode)ptVariant->tGraphicsState.ulDepthMode);
        depthDescriptor.depthWriteEnabled = ptVariant->tGraphicsState.ulDepthWriteEnabled ? YES : NO;

        MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = vertexFunction;
        pipelineDescriptor.fragmentFunction = fragmentFunction;
        pipelineDescriptor.vertexDescriptor = vertexDescriptor;
        pipelineDescriptor.rasterSampleCount = 1;

        uint32_t uColorAttachmentCount = ptRenderPassLayout->_uAttachmentCount;
        if(ptRenderPassLayout->tDesc.tDepthTargetFormat != PL_FORMAT_UNKNOWN)
        {
            pipelineDescriptor.depthAttachmentPixelFormat = pl__metal_format(ptLayout->tDesc.tDepthTargetFormat);
            uColorAttachmentCount--;
        }
        for(uint32_t j = 0; j < uColorAttachmentCount; j++)
        {
            if(j == 0)
            {
                pipelineDescriptor.colorAttachments[0].pixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[j].tFormat);
                pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
                pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
                pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
                pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
                pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
                pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
            }
            else
            {
                pipelineDescriptor.colorAttachments[j].pixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[j].tFormat);
                pipelineDescriptor.colorAttachments[j].blendingEnabled = NO;
            }
        }
        // pipelineDescriptor.stencilAttachmentPixelFormat = ptMetalRenderPass->ptRenderPassDescriptor.stencilAttachment.texture.pixelFormat;

        const plMetalShader tMetalShader = {
            .tDepthStencilState   = [ptMetalDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor],
            .tRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error],
            .tCullMode            = pl__metal_cull(ptVariant->tGraphicsState.ulCullMode)
        };

        if (error != nil)
            NSLog(@"Error: failed to create Metal pipeline state: %@", error);

        plShaderHandle tVariantHandle = {
            .uGeneration = ++ptGraphics->sbtShaderGenerations[uNewResourceIndex],
            .uIndex = uNewResourceIndex
        };
        
        const uint64_t ulVariantHash = pl_hm_hash(ptVariant->pTempConstantData, uTotalConstantSize, ptVariant->tGraphicsState.ulValue);
        pl_hm_insert(&tShader.tVariantHashmap, ulVariantHash, pl_sb_size(tShader._sbtVariantHandles));
        pl_sb_push(tShader._sbtVariantHandles, tVariantHandle);

        ptGraphics->sbtShadersCold[uNewResourceIndex] = tShader;

        if(i == 0)
        {
            ptMetalShader->tDepthStencilState = tMetalShader.tDepthStencilState;
            ptMetalShader->tRenderPipelineState = tMetalShader.tRenderPipelineState;
            ptMetalShader->tCullMode = tMetalShader.tCullMode;
        }
        else
        {
            ptMetalGraphics->sbtShadersHot[uNewResourceIndex] = tMetalShader;
            ptGraphics->sbtShadersCold[uNewResourceIndex]._sbtVariantHandles = NULL;
            memset(&ptGraphics->sbtShadersCold[uNewResourceIndex].tVariantHashmap, 0, sizeof(plHashMap));
        }
    }
    pl_temp_allocator_reset(&ptMetalGraphics->tTempAllocator);
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
    ptGraphics->tSwapchain.bVSync = true;
    pl_sb_resize(ptGraphics->tSwapchain.sbtSwapchainTextureViews, 2);

    // create command queue
    ptMetalGraphics->tCmdQueue = [ptMetalDevice->tDevice newCommandQueue];

    // ptGraphics->tSwapchain.tMsaaSamples = 1;
    // if([ptMetalDevice->tDevice supportsTextureSampleCount:8])
    //    ptGraphics->tSwapchain.tMsaaSamples = 8;

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

    // staging cached
    static plDeviceAllocatorData tStagingCachedData = {0};
    tStagingCachedData.ptDevice = &ptGraphics->tDevice;
    ptGraphics->tDevice.tStagingCachedAllocator.allocate = pl_allocate_staging_uncached;
    ptGraphics->tDevice.tStagingCachedAllocator.free = pl_free_staging_uncached;
    ptGraphics->tDevice.tStagingCachedAllocator.blocks = pl_get_allocator_blocks;
    ptGraphics->tDevice.tStagingCachedAllocator.ranges = pl_get_allocator_ranges;
    ptGraphics->tDevice.tStagingCachedAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tStagingCachedData;

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = MTLStorageModeShared;
    ptHeapDescriptor.size        = PL_ARGUMENT_BUFFER_HEAP_SIZE;
    ptHeapDescriptor.type        = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
    ptMetalDevice->tDescriptorHeap = [ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];

    pl_sb_resize(ptGraphics->sbtGarbage, ptGraphics->uFramesInFlight);
    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {
            .tFrameBoundarySemaphore = dispatch_semaphore_create(1),
            .tFence = [ptMetalDevice->tDevice newFence]
        };
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        static char atNameBuffer[PL_MAX_NAME_LENGTH] = {0};
        pl_sprintf(atNameBuffer, "D-BUF-F%d-0", (int)i);
        tFrame.sbtDynamicBuffers[0].tMemory = ptGraphics->tDevice.tStagingUnCachedAllocator.allocate(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, 0, PL_DEVICE_ALLOCATION_BLOCK_SIZE, 0,atNameBuffer);
        tFrame.sbtDynamicBuffers[0].tBuffer = [(id<MTLHeap>)tFrame.sbtDynamicBuffers[0].tMemory.uHandle newBufferWithLength:PL_DEVICE_ALLOCATION_BLOCK_SIZE options:MTLResourceStorageModeShared offset:0];
        tFrame.sbtDynamicBuffers[0].tBuffer.label = [NSString stringWithUTF8String:"dynamic"];
        
        plMetalBuffer tArgumentBuffer = {
            .tBuffer = [ptMetalDevice->tDescriptorHeap newBufferWithLength:PL_DYNAMIC_ARGUMENT_BUFFER_SIZE options:MTLResourceStorageModeShared offset:ptMetalDevice->ulDescriptorHeapOffset]
        };
        ptMetalDevice->ulDescriptorHeapOffset += PL_DYNAMIC_ARGUMENT_BUFFER_SIZE;
        ptMetalDevice->ulDescriptorHeapOffset = PL__ALIGN_UP(ptMetalDevice->ulDescriptorHeapOffset, 256);

        pl_sb_push(tFrame.sbtArgumentBuffers, tArgumentBuffer);
        pl_sb_push(ptMetalGraphics->sbFrames, tFrame);
    }

    pl_create_main_render_pass_layout(&ptGraphics->tDevice);
    pl_create_main_render_pass(&ptGraphics->tDevice);
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

    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
    pl__garbage_collect(ptGraphics);

    pl_end_profile_sample();
}

static bool
pl_begin_frame(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    // Wait until the inflight command buffer has completed its work
    ptGraphics->tSwapchain.uCurrentImageIndex = ptGraphics->uCurrentFrameIndex;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    ptFrame->uCurrentArgumentBuffer = 0;
    dispatch_semaphore_wait(ptFrame->tFrameBoundarySemaphore, DISPATCH_TIME_FOREVER);

    pl__garbage_collect(ptGraphics);
    
    plIO* ptIOCtx = pl_get_io();
    ptMetalGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;
    
    // get next drawable
    ptMetalGraphics->tCurrentDrawable = [ptMetalGraphics->pMetalLayer nextDrawable];

    if(!ptMetalGraphics->tCurrentDrawable)
    {
        pl_end_profile_sample();
        return false;
    }

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

static plCommandBuffer
pl_begin_command_recording(plGraphics* ptGraphics, const plBeginCommandInfo* ptBeginInfo)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = [ptMetalGraphics->tCmdQueue commandBufferWithUnretainedReferences];
    plCommandBuffer tCommandBuffer = {
        ._pInternal = tCmdBuffer
    };

    if(ptBeginInfo)
    {
        tCommandBuffer.tBeginInfo = *ptBeginInfo;
        for(uint32_t i = 0; i < ptBeginInfo->uWaitSemaphoreCount; i++)
        {
            if(ptMetalGraphics->sbtSemaphoresHot[ptBeginInfo->atWaitSempahores[i].uIndex].tEvent)
            {
                [tCmdBuffer encodeWaitForEvent:ptMetalGraphics->sbtSemaphoresHot[ptBeginInfo->atWaitSempahores[i].uIndex].tEvent value:ptBeginInfo->auWaitSemaphoreValues[i]];
            }
            else
            {
                [tCmdBuffer encodeWaitForEvent:ptMetalGraphics->sbtSemaphoresHot[ptBeginInfo->atWaitSempahores[i].uIndex].tSharedEvent value:ptBeginInfo->auWaitSemaphoreValues[i]];
            }
        }
    }

    return tCommandBuffer;
}

static void
pl_end_command_recording(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer)
{
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptCmdBuffer->_pInternal;
    [tCmdBuffer enqueue];
}

static bool
pl_present(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptCmdBuffer->_pInternal;

    [tCmdBuffer presentDrawable:ptMetalGraphics->tCurrentDrawable];

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            if(ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent)
            {
                [tCmdBuffer encodeSignalEvent:ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [tCmdBuffer encodeSignalEvent:ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }
    
    ptFrame->uCurrentBufferIndex = UINT32_MAX;

    dispatch_semaphore_t semaphore = ptFrame->tFrameBoundarySemaphore;
    [tCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
        // GPU work is complete
        // Signal the semaphore to start the CPU work
        dispatch_semaphore_signal(semaphore);
    }];

    [tCmdBuffer commit];

    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
    ptCmdBuffer->_pInternal = NULL;
    return true;
}

static plRenderEncoder
pl_begin_render_pass(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, plRenderPassHandle tPass)
{
    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[tPass.uIndex];
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[tPass.uIndex];
    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];
    
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptCmdBuffer->_pInternal;
    id<MTLRenderCommandEncoder> tRenderEncoder = nil;

    if(ptRenderPass->bSwapchain)
    {
        ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].texture = ptMetalGraphics->tCurrentDrawable.texture;
        tRenderEncoder = [tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->ptRenderPassDescriptor];
        pl_new_draw_frame_metal(ptMetalRenderPass->ptRenderPassDescriptor);
    }
    else
    {

        const plRenderPassAttachments* ptAttachment = &ptMetalRenderPass->atFrameBuffers[0];

        uint32_t uColorAttachmentCount = ptLayout->_uAttachmentCount;

        if(ptLayout->tDesc.tDepthTargetFormat != PL_FORMAT_UNKNOWN)
        {
            const uint32_t uDepthIndex = ptGraphics->sbtTextureViewsCold[ptAttachment->atViewAttachments[0].uIndex].tTexture.uIndex;
            ptMetalRenderPass->ptRenderPassDescriptor.depthAttachment.texture = ptMetalGraphics->sbtTexturesHot[uDepthIndex].tTexture;
            uColorAttachmentCount--;
        }
        
        for(uint32_t i = 0; i < uColorAttachmentCount; i++)
        {
            const uint32_t uColorIndex = ptGraphics->sbtTextureViewsCold[ptAttachment->atViewAttachments[i+1].uIndex].tTexture.uIndex;
            ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[i].texture = ptMetalGraphics->sbtTexturesHot[uColorIndex].tTexture;
        }

        tRenderEncoder = [tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->ptRenderPassDescriptor];
    }

    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    // [tRenderEncoder waitForFence:ptFrame->tFence beforeStages:MTLRenderStageFragment];

    plRenderEncoder tEncoder = {
        .ptGraphics        = ptGraphics,
        .tCommandBuffer    = *ptCmdBuffer,
        ._pInternal        = tRenderEncoder,
        .tRenderPassHandle = tPass
    };
    
    return tEncoder;
}

static void
pl_end_render_pass(plRenderEncoder* ptEncoder)
{
    id<MTLRenderCommandEncoder> tRenderEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;
    [tRenderEncoder endEncoding];
}

static void
pl_submit_command_buffer(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptCmdBuffer->_pInternal;

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {

            if(ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent)
            {
                [tCmdBuffer encodeSignalEvent:ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [tCmdBuffer encodeSignalEvent:ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }

    [tCmdBuffer commit];
}

static void
pl_submit_command_buffer_blocking(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptCmdBuffer->_pInternal;

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            if(ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent)
            {
                [tCmdBuffer encodeSignalEvent:ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [tCmdBuffer encodeSignalEvent:ptMetalGraphics->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }

    [tCmdBuffer commit];
    [tCmdBuffer waitUntilCompleted];
    ptCmdBuffer->_pInternal = NULL;
}

static plBlitEncoder
pl_begin_blit_pass(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptCmdBuffer->_pInternal;

    id<MTLBlitCommandEncoder> tBlitEncoder = [tCmdBuffer blitCommandEncoder];
    plBlitEncoder tEncoder = {
        .ptGraphics     = ptGraphics,
        .tCommandBuffer = *ptCmdBuffer,
        ._pInternal     = tBlitEncoder
    };

    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
    // [tBlitEncoder waitForFence:ptFrame->tFence];
    
    return tEncoder;
}

static void
pl_end_blit_pass(plBlitEncoder* ptEncoder)
{
    id<MTLBlitCommandEncoder> tBlitEncoder = (id<MTLBlitCommandEncoder>)ptEncoder->_pInternal;
    [tBlitEncoder endEncoding];
}

static plComputeEncoder
pl_begin_compute_pass(plGraphics* ptGraphics, plCommandBuffer* ptCmdBuffer)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptCmdBuffer->_pInternal;

    id<MTLComputeCommandEncoder> tComputeEncoder = [tCmdBuffer computeCommandEncoder];
    plComputeEncoder tEncoder = {
        .ptGraphics     = ptGraphics,
        .tCommandBuffer = *ptCmdBuffer,
        ._pInternal     = tComputeEncoder
    };
    return tEncoder;
}

static void
pl_end_compute_pass(plComputeEncoder* ptEncoder)
{
    id<MTLComputeCommandEncoder> tComputeEncoder = (id<MTLComputeCommandEncoder>)ptEncoder->_pInternal;
    [tComputeEncoder endEncoding];
}

static void
pl_dispatch(plComputeEncoder* ptEncoder, uint32_t uDispatchCount, plDispatch* atDispatches)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    id<MTLComputeCommandEncoder> tComputeEncoder = (id<MTLComputeCommandEncoder>)ptEncoder->_pInternal;

    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        plMetalComputeShader* ptComputeShader = &ptMetalGraphics->sbtComputeShadersHot[ptDispatch->uShaderVariant];
        plMetalBindGroup* ptBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptDispatch->uBindGroup0];
        [tComputeEncoder setComputePipelineState:ptComputeShader->tPipelineState];

        for(uint32_t k = 0; k < ptBindGroup->tLayout.uBufferCount; k++)
        {
            const plBufferHandle tBufferHandle = ptBindGroup->tLayout.aBuffers[k].tBuffer;
            [tComputeEncoder useHeap:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tHeap];
            [tComputeEncoder useResource:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite]; 
        }

        [tComputeEncoder setBuffer:ptBindGroup->tShaderArgumentBuffer
            offset:0
            atIndex:0];

        MTLSize tGridSize = MTLSizeMake(ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
        MTLSize tThreadsPerGroup = MTLSizeMake(ptDispatch->uThreadPerGroupX, ptDispatch->uThreadPerGroupY, ptDispatch->uThreadPerGroupZ);
        [tComputeEncoder dispatchThreadgroups:tGridSize threadsPerThreadgroup:tThreadsPerGroup];
    }
}

static void
pl_draw_subpass(plRenderEncoder* ptEncoder, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    id<MTLRenderCommandEncoder> tRenderEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
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
        [tRenderEncoder setScissorRect:tScissorRect];

        MTLViewport tViewport = {
            .originX = ptArea->tViewport.fX,
            .originY = ptArea->tViewport.fY,
            .width   = ptArea->tViewport.fWidth,
            .height  = ptArea->tViewport.fHeight,
            .znear   = 0,
            .zfar    = 1.0
        };
        [tRenderEncoder setViewport:tViewport];

        const uint32_t uTokens = pl_sb_size(ptStream->sbtStream);
        uint32_t uCurrentStreamIndex = 0;
        uint32_t uTriangleCount = 0;
        uint32_t uIndexBuffer = 0;
        uint32_t uIndexBufferOffset = 0;
        uint32_t uVertexBufferOffset = 0;
        uint32_t uDynamicBufferOffset = 0;
        uint32_t uInstanceStart = 0;
        uint32_t uInstanceCount = 1;

        while(uCurrentStreamIndex < uTokens)
        {
            const uint32_t uDirtyMask = ptStream->sbtStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                plMetalShader* ptMetalShader = &ptMetalGraphics->sbtShadersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                [tRenderEncoder setCullMode:ptMetalShader->tCullMode];
                [tRenderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
                [tRenderEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
                [tRenderEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
                [tRenderEncoder setTriangleFillMode:ptMetalShader->tFillMode];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                uDynamicBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
            {

                [tRenderEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer
                    offset:0
                    atIndex:4];

                [tRenderEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer
                    offset:0
                    atIndex:4];

                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                [tRenderEncoder setVertexBufferOffset:uDynamicBufferOffset atIndex:4];
                [tRenderEncoder setFragmentBufferOffset:uDynamicBufferOffset atIndex:4];
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uBufferCount; k++)
                {
                    const plBufferHandle tBufferHandle = ptMetalBindGroup->tLayout.aBuffers[k].tBuffer;
                    [tRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tHeap stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aBuffers[k].tStages)];
                    [tRenderEncoder useResource:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer
                        usage:MTLResourceUsageRead
                        stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aBuffers[k].tStages)]; 
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptGraphics->sbtTextureViewsCold[ptMetalBindGroup->tLayout.aTextures[k].tTextureView.uIndex].tTexture;
                    id<MTLHeap> tHeap = ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tHeap;
                    [tRenderEncoder useHeap:tHeap stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aTextures[k].tStages)];
                    [tRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture
                        usage:MTLResourceUsageRead
                        stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aTextures[k].tStages)]; 
                }

                [tRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:ptMetalBindGroup->uOffset
                    atIndex:3];

                [tRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:ptMetalBindGroup->uOffset
                    atIndex:3];
                uCurrentStreamIndex++;
            }
     
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uBufferCount; k++)
                {
                    const plBufferHandle tBufferHandle = ptMetalBindGroup->tLayout.aBuffers[k].tBuffer;
                    [tRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tHeap stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aBuffers[k].tStages)];
                    [tRenderEncoder useResource:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer
                        usage:MTLResourceUsageRead
                        stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aBuffers[k].tStages)]; 
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptGraphics->sbtTextureViewsCold[ptMetalBindGroup->tLayout.aTextures[k].tTextureView.uIndex].tTexture;
                    id<MTLHeap> tHeap = ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tHeap;
                    [tRenderEncoder useHeap:tHeap stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aTextures[k].tStages)];
                    [tRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture
                        usage:MTLResourceUsageRead
                        stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aTextures[k].tStages)];  
                }

                [tRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:ptMetalBindGroup->uOffset
                    atIndex:2];

                [tRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:ptMetalBindGroup->uOffset
                    atIndex:2];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uBufferCount; k++)
                {
                    const plBufferHandle tBufferHandle = ptMetalBindGroup->tLayout.aBuffers[k].tBuffer;
                    [tRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tHeap stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aBuffers[k].tStages)];
                    [tRenderEncoder useResource:ptMetalGraphics->sbtBuffersHot[tBufferHandle.uIndex].tBuffer
                        usage:MTLResourceUsageRead
                        stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aBuffers[k].tStages)]; 
                }


                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureCount; k++)
                {
                    
                    const plTextureHandle tTextureHandle = ptGraphics->sbtTextureViewsCold[ptMetalBindGroup->tLayout.aTextures[k].tTextureView.uIndex].tTexture;
                    id<MTLHeap> tHeap = ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tHeap;
                    [tRenderEncoder useHeap:tHeap stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aTextures[k].tStages)];
                    [tRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture
                        usage:MTLResourceUsageRead
                        stages:pl__metal_stage_flags(ptMetalBindGroup->tLayout.aTextures[k].tStages)];  
                }

                [tRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:ptMetalBindGroup->uOffset
                    atIndex:1];

                [tRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer
                    offset:ptMetalBindGroup->uOffset
                    atIndex:1];

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
            if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
            {
                uIndexBuffer = ptStream->sbtStream[uCurrentStreamIndex];
                if(uIndexBuffer != UINT32_MAX)
                    [tRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]].tHeap stages:MTLRenderStageVertex];
                
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
            {
                [tRenderEncoder useHeap:ptMetalGraphics->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]].tHeap stages:MTLRenderStageVertex];
                [tRenderEncoder setVertexBuffer:ptMetalGraphics->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer
                    offset:0
                    atIndex:0];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
            {
                uTriangleCount = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_START)
            {
                uInstanceStart = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
            {
                uInstanceCount = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uIndexBuffer == UINT32_MAX)
            {
                [tRenderEncoder drawPrimitives:MTLPrimitiveTypeTriangle 
                    vertexStart:uVertexBufferOffset
                    vertexCount:uTriangleCount * 3
                    instanceCount:uInstanceCount
                    baseInstance:uInstanceStart
                    ];
            }
            else
            {
                [tRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
                    indexCount:uTriangleCount * 3
                    indexType:MTLIndexTypeUInt32
                    indexBuffer:ptMetalGraphics->sbtBuffersHot[uIndexBuffer].tBuffer
                    indexBufferOffset:uIndexBufferOffset * sizeof(uint32_t)
                    instanceCount:uInstanceCount
                    baseVertex:uVertexBufferOffset
                    baseInstance:uInstanceStart
                    ];
            }
        }
    }
    pl_end_profile_sample();
}

static void
pl_cleanup(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;

    pl_cleanup_metal();

    for(uint32_t i = 0; i < pl_sb_size(ptMetalGraphics->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptMetalGraphics->sbFrames[i];
        pl_sb_free(ptFrame->sbtDynamicBuffers);
        pl_sb_free(ptFrame->sbtArgumentBuffers);
    }

    pl_sb_free(ptMetalGraphics->sbFrames);
    pl_sb_free(ptMetalGraphics->sbtTexturesHot);
    pl_sb_free(ptMetalGraphics->sbtSamplersHot);
    pl_sb_free(ptMetalGraphics->sbtBindGroupsHot);
    pl_sb_free(ptMetalGraphics->sbtBuffersHot);
    pl_sb_free(ptMetalGraphics->sbtShadersHot);
    pl_sb_free(ptMetalGraphics->sbtPipelineEntries);
    pl_sb_free(ptMetalGraphics->sbFrames);
    pl_sb_free(ptMetalGraphics->sbtRenderPassesHot);
    pl_sb_free(ptMetalGraphics->sbtRenderPassLayoutsHot);
    pl_sb_free(ptMetalGraphics->sbtComputeShadersHot);
    pl_sb_free(ptMetalGraphics->sbtSemaphoresHot);
    pl__cleanup_common_graphics(ptGraphics);
}

static void
pl_draw_lists(plGraphics* ptGraphics, plRenderEncoder tEncoder, uint32_t uListCount, plDrawList* atLists)
{
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)tEncoder.tCommandBuffer._pInternal;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLRenderCommandEncoder> tRenderEncoder = (id<MTLRenderCommandEncoder>)tEncoder._pInternal;
    id<MTLDevice> tDevice = ptMetalDevice->tDevice;
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[tEncoder.tRenderPassHandle.uIndex];

    plIO* ptIOCtx = pl_get_io();
    for(uint32_t i = 0; i < uListCount; i++)
    {
        pl_submit_metal_drawlist(&atLists[i], ptIOCtx->afMainViewportSize[0], ptIOCtx->afMainViewportSize[1], tRenderEncoder, tCmdBuffer, ptMetalRenderPass->ptRenderPassDescriptor);
    }
}

static void
pl__submit_3d_drawlist(plDrawList3D* ptDrawlist, plRenderEncoder tEncoder, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags, uint32_t uMSAASampleCount)
{
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)tEncoder.tCommandBuffer._pInternal;
    plGraphics* ptGfx = ptDrawlist->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGfx->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptGfx->tDevice._pInternalData;
    id<MTLRenderCommandEncoder> tRenderEncoder = (id<MTLRenderCommandEncoder>)tEncoder._pInternal;

    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[tEncoder.tRenderPassHandle.uIndex];
    plMetalPipelineEntry* ptPipelineEntry = pl__get_3d_pipelines(ptGfx, tFlags, ptMetalRenderPass->ptRenderPassDescriptor.colorAttachments[0].texture.sampleCount, ptMetalRenderPass->ptRenderPassDescriptor);

    const float fAspectRatio = fWidth / fHeight;

    const uint32_t uTotalIdxBufSzNeeded = sizeof(uint32_t) * (pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) + pl_sb_size(ptDrawlist->sbtLineIndexBuffer));
    const uint32_t uSolidVtxBufSzNeeded = sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);
    const uint32_t uLineVtxBufSzNeeded = sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer);

    if(uTotalIdxBufSzNeeded == 0)
        return;

    plTrackedMetalBuffer* tIndexBuffer = pl__dequeue_reusable_buffer(ptGfx, uTotalIdxBufSzNeeded);
    plTrackedMetalBuffer* tVertexBuffer = pl__dequeue_reusable_buffer(ptGfx, uLineVtxBufSzNeeded + uSolidVtxBufSzNeeded);
    uint32_t uVertexOffset = 0;
    uint32_t uIndexOffset = 0;

    [tRenderEncoder setDepthStencilState:ptPipelineEntry->tDepthStencilState];
    [tRenderEncoder setCullMode:(tFlags & PL_PIPELINE_FLAG_FRONT_FACE_CW)];
    [tRenderEncoder setTriangleFillMode:MTLTriangleFillModeFill];
    int iCullMode = MTLCullModeNone;
    if(tFlags & PL_PIPELINE_FLAG_CULL_FRONT) iCullMode = MTLCullModeFront;
    if(tFlags & PL_PIPELINE_FLAG_CULL_BACK) iCullMode |= MTLCullModeBack;
    [tRenderEncoder setCullMode:iCullMode];
    [tRenderEncoder setFrontFacingWinding:(tFlags & PL_PIPELINE_FLAG_FRONT_FACE_CW) ? MTLWindingClockwise : MTLWindingCounterClockwise];

    if(pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) > 0)
    {
        memcpy(tVertexBuffer.buffer.contents, ptDrawlist->sbtSolidVertexBuffer, uSolidVtxBufSzNeeded);
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);
        memcpy(tIndexBuffer.buffer.contents, ptDrawlist->sbtSolidIndexBuffer, uIdxBufSzNeeded);

        [tRenderEncoder setVertexBytes:ptMVP length:sizeof(plMat4) atIndex:1 ];
        
        [tRenderEncoder setVertexBuffer:tVertexBuffer.buffer offset:uVertexOffset atIndex:0];
        [tRenderEncoder setRenderPipelineState:ptPipelineEntry->tSolidRenderPipelineState];
        [tRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) indexType:MTLIndexTypeUInt32 indexBuffer:tIndexBuffer.buffer indexBufferOffset:uIndexOffset];

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

        [tRenderEncoder setVertexBytes:&b length:sizeof(struct UniformData) atIndex:1 ];
        [tRenderEncoder setVertexBuffer:tVertexBuffer.buffer offset:uVertexOffset atIndex:0];
        [tRenderEncoder setRenderPipelineState:ptPipelineEntry->tLineRenderPipelineState];
        [tRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:pl_sb_size(ptDrawlist->sbtLineIndexBuffer) indexType:MTLIndexTypeUInt32 indexBuffer:tIndexBuffer.buffer indexBufferOffset:uIndexOffset];
    }

    [tCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> tCmdBuffer2)
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

static MTLDataType
pl__metal_data_type(plDataType tType)
{
    switch(tType)
    {

        case PL_DATA_TYPE_BOOL:           return MTLDataTypeBool;
        case PL_DATA_TYPE_FLOAT:          return MTLDataTypeFloat;
        case PL_DATA_TYPE_UNSIGNED_BYTE:  return MTLDataTypeUChar;
        case PL_DATA_TYPE_BYTE:           return MTLDataTypeChar;
        case PL_DATA_TYPE_UNSIGNED_SHORT: return MTLDataTypeUShort;
        case PL_DATA_TYPE_SHORT:          return MTLDataTypeShort;
        case PL_DATA_TYPE_UNSIGNED_INT:   return MTLDataTypeUInt;
        case PL_DATA_TYPE_INT:            return MTLDataTypeInt;
        case PL_DATA_TYPE_UNSIGNED_LONG:  return MTLDataTypeULong;
        case PL_DATA_TYPE_LONG:           return MTLDataTypeLong;

        case PL_DATA_TYPE_BOOL2:           return MTLDataTypeBool2;
        case PL_DATA_TYPE_FLOAT2:          return MTLDataTypeFloat2;
        case PL_DATA_TYPE_UNSIGNED_BYTE2:  return MTLDataTypeUChar2;
        case PL_DATA_TYPE_BYTE2:           return MTLDataTypeChar2;
        case PL_DATA_TYPE_UNSIGNED_SHORT2: return MTLDataTypeUShort2;
        case PL_DATA_TYPE_SHORT2:          return MTLDataTypeShort2;
        case PL_DATA_TYPE_UNSIGNED_INT2:   return MTLDataTypeUInt2;
        case PL_DATA_TYPE_INT2:            return MTLDataTypeInt2;
        case PL_DATA_TYPE_UNSIGNED_LONG2:  return MTLDataTypeULong2;
        case PL_DATA_TYPE_LONG2:           return MTLDataTypeLong2;

        case PL_DATA_TYPE_BOOL3:           return MTLDataTypeBool3;
        case PL_DATA_TYPE_FLOAT3:          return MTLDataTypeFloat3;
        case PL_DATA_TYPE_UNSIGNED_BYTE3:  return MTLDataTypeUChar3;
        case PL_DATA_TYPE_BYTE3:           return MTLDataTypeChar3;
        case PL_DATA_TYPE_UNSIGNED_SHORT3: return MTLDataTypeUShort3;
        case PL_DATA_TYPE_SHORT3:          return MTLDataTypeShort3;
        case PL_DATA_TYPE_UNSIGNED_INT3:   return MTLDataTypeUInt3;
        case PL_DATA_TYPE_INT3:            return MTLDataTypeInt3;
        case PL_DATA_TYPE_UNSIGNED_LONG3:  return MTLDataTypeULong3;
        case PL_DATA_TYPE_LONG3:           return MTLDataTypeLong3;

        case PL_DATA_TYPE_BOOL4:           return MTLDataTypeBool4;
        case PL_DATA_TYPE_FLOAT4:          return MTLDataTypeFloat4;
        case PL_DATA_TYPE_UNSIGNED_BYTE4:  return MTLDataTypeUChar4;
        case PL_DATA_TYPE_BYTE4:           return MTLDataTypeChar4;
        case PL_DATA_TYPE_UNSIGNED_SHORT4: return MTLDataTypeUShort4;
        case PL_DATA_TYPE_SHORT4:          return MTLDataTypeShort4;
        case PL_DATA_TYPE_UNSIGNED_INT4:   return MTLDataTypeUInt4;
        case PL_DATA_TYPE_INT4:            return MTLDataTypeInt4;
        case PL_DATA_TYPE_UNSIGNED_LONG4:  return MTLDataTypeULong4;
        case PL_DATA_TYPE_LONG4:           return MTLDataTypeLong4;
    }

    PL_ASSERT(false && "Unsupported data type");
    return 0;
}

static MTLStoreAction
pl__metal_store_op(plStoreOp tOp)
{
    switch(tOp)
    {
        case PL_STORE_OP_STORE:               return MTLStoreActionStore;
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
        case PL_WRAP_MODE_WRAP:   return MTLSamplerAddressModeRepeat;
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

static MTLRenderStages
pl__metal_stage_flags(plStageFlags tFlags)
{
    MTLRenderStages tResult = 0;

    if(tFlags & PL_STAGE_VERTEX)   tResult |= MTLRenderStageVertex;
    if(tFlags & PL_STAGE_PIXEL)    tResult |= MTLRenderStageFragment;
    // if(tFlags & PL_STAGE_COMPUTE)  tResult |= VK_SHADER_STAGE_COMPUTE_BIT; // not needed

    return tResult;
}

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
    backing.label = [NSString stringWithUTF8String:"3d drawing"];
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

static void
pl__garbage_collect(plGraphics* ptGraphics)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal*   ptMetalDevice = ptGraphics->tDevice._pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);


    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPasses); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPasses[i].uIndex;
        plMetalRenderPass* ptMetalResource = &ptMetalGraphics->sbtRenderPassesHot[iResourceIndex];
        [ptMetalResource->ptRenderPassDescriptor release];
        ptMetalResource->ptRenderPassDescriptor = nil;
        pl_sb_push(ptGraphics->sbtRenderPassFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPassLayouts); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPassLayouts[i].uIndex;
        plMetalRenderPassLayout* ptMetalResource = &ptMetalGraphics->sbtRenderPassLayoutsHot[iResourceIndex];
        pl_sb_push(ptGraphics->sbtRenderPassLayoutFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtShaders[i].uIndex;
        plShader* ptResource = &ptGraphics->sbtShadersCold[iResourceIndex];

        for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
        {
            const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
            plMetalShader* ptVariantMetalResource = &ptMetalGraphics->sbtShadersHot[iVariantIndex];
            [ptVariantMetalResource->tDepthStencilState release];
            [ptVariantMetalResource->tRenderPipelineState release];
            ptVariantMetalResource->tDepthStencilState = nil;
            ptVariantMetalResource->tRenderPipelineState = nil;
            if(ptVariantMetalResource->library)
            {
                [ptVariantMetalResource->library release];
                ptVariantMetalResource->library = nil;
            }
            pl_sb_push(ptGraphics->sbtShaderFreeIndices, iVariantIndex);
        }
        pl_sb_free(ptResource->_sbtVariantHandles);
        pl_hm_free(&ptResource->tVariantHashmap);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
        plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[iResourceIndex];

        for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
        {
            const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
            plMetalComputeShader* ptVariantMetalResource = &ptMetalGraphics->sbtComputeShadersHot[iVariantIndex];
            [ptVariantMetalResource->tPipelineState release];
            ptVariantMetalResource->tPipelineState = nil;
            if(ptVariantMetalResource->library)
            {
                [ptVariantMetalResource->library release];
                ptVariantMetalResource->library = nil;
            }
            pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, iVariantIndex);
        }
        pl_sb_free(ptResource->_sbtVariantHandles);
        pl_hm_free(&ptResource->tVariantHashmap);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBindGroups); i++)
    {
        const uint32_t iBindGroupIndex = ptGarbage->sbtBindGroups[i].uIndex;
        plMetalBindGroup* ptMetalResource = &ptMetalGraphics->sbtBindGroupsHot[iBindGroupIndex];
        [ptMetalResource->tShaderArgumentBuffer release];
        ptMetalResource->tShaderArgumentBuffer = nil;
        pl_sb_push(ptGraphics->sbtBindGroupFreeIndices, iBindGroupIndex);
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
        else if(ptGarbage->sbtMemory[i].ptInst == ptGraphics->tDevice.tStagingCachedAllocator.ptInst)
            ptGraphics->tDevice.tStagingCachedAllocator.free(ptGraphics->tDevice.tStagingCachedAllocator.ptInst, &ptGarbage->sbtMemory[i]);
    }

    plDeviceAllocatorData* ptUnCachedAllocatorData = (plDeviceAllocatorData*)ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst;

    plIO* ptIO = pl_get_io();
    for(uint32_t i = 0; i < pl_sb_size(ptUnCachedAllocatorData->sbtNodes); i++)
    {
        plDeviceAllocationRange* ptNode = &ptUnCachedAllocatorData->sbtNodes[i];
        plDeviceAllocationBlock* ptBlock = &ptUnCachedAllocatorData->sbtBlocks[ptNode->ulBlockIndex];

        if(ptBlock->ulAddress == 0)
        {
            continue;
        }
        if(ptNode->ulUsedSize == 0 && ptIO->dTime - ptBlock->dLastTimeUsed > 1.0)
        {
            ptGraphics->szHostMemoryInUse -= ptBlock->ulSize;
            id<MTLHeap> tMetalHeap = (id<MTLHeap>)ptBlock->ulAddress;
            [tMetalHeap release];
            tMetalHeap = nil;

            ptBlock->ulAddress = 0;
            pl_sb_push(ptUnCachedAllocatorData->sbtFreeBlockIndices, ptNode->ulBlockIndex);
        }
        else if(ptNode->ulUsedSize != 0)
            ptBlock->dLastTimeUsed = ptIO->dTime;
    }

    pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtTextureViews);
    pl_sb_reset(ptGarbage->sbtShaders);
    pl_sb_reset(ptGarbage->sbtComputeShaders);
    pl_sb_reset(ptGarbage->sbtRenderPasses);
    pl_sb_reset(ptGarbage->sbtRenderPassLayouts);
    pl_sb_reset(ptGarbage->sbtMemory);
    pl_sb_reset(ptGarbage->sbtBuffers);
    pl_sb_reset(ptGarbage->sbtBindGroups);
    pl_end_profile_sample();
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
    ptData->ptDevice->ptGraphics->szLocalMemoryInUse += tBlock.ulSize;
    
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
            ptData->ptDevice->ptGraphics->szLocalMemoryInUse -= ptBlock->ulSize;
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
        ptHeapDescriptor.size        = PL_DEVICE_BUDDY_BLOCK_SIZE;
        ptHeapDescriptor.type        = MTLHeapTypePlacement;
        ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
        ptBlock->ulAddress = (uint64_t)[ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
        tAllocation.uHandle = (uint64_t)ptBlock->ulAddress;
        ptData->ptDevice->ptGraphics->szLocalMemoryInUse += ptBlock->ulSize;
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
        if(ptNode->ulUsedSize == 0 && ptNode->ulTotalSize >= ulSize && ptBlock->ulAddress != 0)
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

    uint32_t uIndex = UINT32_MAX;
    if(pl_sb_size(ptData->sbtFreeBlockIndices) > 0)
    {
        uIndex = pl_sb_pop(ptData->sbtFreeBlockIndices);
    }
    else
    {
        uIndex = pl_sb_size(ptData->sbtBlocks);
        pl_sb_add(ptData->sbtNodes);
        pl_sb_add(ptData->sbtBlocks);
    }

    plDeviceAllocationBlock tBlock = {
        .ulAddress = 0,
        .ulSize    = pl_maxu((uint32_t)ulSize, PL_DEVICE_ALLOCATION_BLOCK_SIZE)
    };

    plDeviceAllocationRange tRange = {
        .ulOffset     = 0,
        .ulUsedSize   = ulSize,
        .ulTotalSize  = tBlock.ulSize,
        .ulBlockIndex = uIndex
    };
    pl_sprintf(tRange.acName, "%s", pcName);

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = MTLStorageModeShared;
    ptHeapDescriptor.size = tBlock.ulSize;
    ptHeapDescriptor.type = MTLHeapTypePlacement;
    ptData->ptDevice->ptGraphics->szHostMemoryInUse += tBlock.ulSize;

    tBlock.ulAddress = (uint64_t)[ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    tAllocation.uHandle = tBlock.ulAddress;

    ptData->sbtNodes[uIndex] = tRange;
    ptData->sbtBlocks[uIndex] = tBlock;
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
            break;
        }
    }
}

static void
pl_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;

    ptGraphics->sbtBufferGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtBufferFreeIndices, tHandle.uIndex);

    [ptMetalGraphics->sbtBuffersHot[tHandle.uIndex].tBuffer release];
    ptMetalGraphics->sbtBuffersHot[tHandle.uIndex].tBuffer = nil;

    plBuffer* ptBuffer = &ptGraphics->sbtBuffersCold[tHandle.uIndex];

    if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalBuddyAllocator.ptInst)
        ptGraphics->tDevice.tLocalBuddyAllocator.free(ptGraphics->tDevice.tLocalBuddyAllocator.ptInst, &ptBuffer->tMemoryAllocation);
    else if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst)
        ptGraphics->tDevice.tLocalDedicatedAllocator.free(ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst, &ptBuffer->tMemoryAllocation);
    else if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingUnCachedAllocator.free(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, &ptBuffer->tMemoryAllocation);
    else if(ptBuffer->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingCachedAllocator.free(ptGraphics->tDevice.tStagingCachedAllocator.ptInst, &ptBuffer->tMemoryAllocation);
}

static void
pl_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;

    pl_sb_push(ptGraphics->sbtTextureFreeIndices, tHandle.uIndex);
    ptGraphics->sbtTextureGenerations[tHandle.uIndex]++;

    plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[tHandle.uIndex];
    [ptMetalTexture->tTexture release];
    ptMetalTexture->tTexture = nil;

    plTexture* ptTexture = &ptGraphics->sbtTexturesCold[tHandle.uIndex];

    if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalBuddyAllocator.ptInst)
        ptGraphics->tDevice.tLocalBuddyAllocator.free(ptGraphics->tDevice.tLocalBuddyAllocator.ptInst, &ptTexture->tMemoryAllocation);
    else if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst)
        ptGraphics->tDevice.tLocalDedicatedAllocator.free(ptGraphics->tDevice.tLocalDedicatedAllocator.ptInst, &ptTexture->tMemoryAllocation);
    else if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingUnCachedAllocator.free(ptGraphics->tDevice.tStagingUnCachedAllocator.ptInst, &ptTexture->tMemoryAllocation);
    else if(ptTexture->tMemoryAllocation.ptInst == ptGraphics->tDevice.tStagingCachedAllocator.ptInst)
        ptGraphics->tDevice.tStagingCachedAllocator.free(ptGraphics->tDevice.tStagingCachedAllocator.ptInst, &ptTexture->tMemoryAllocation);
}

static void
pl_destroy_texture_view(plDevice* ptDevice, plTextureViewHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;

    ptGraphics->sbtTextureViewGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtTextureViewFreeIndices, tHandle.uIndex);

    plMetalSampler* ptMetalSampler = &ptMetalGraphics->sbtSamplersHot[tHandle.uIndex];
    [ptMetalSampler->tSampler release];
    ptMetalSampler->tSampler = nil;
}

static void
pl_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;
    
    ptGraphics->sbtBindGroupGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtBindGroupFreeIndices, tHandle.uIndex);

    plMetalBindGroup* ptMetalResource = &ptMetalGraphics->sbtBindGroupsHot[tHandle.uIndex];
    [ptMetalResource->tShaderArgumentBuffer release];
    ptMetalResource->tShaderArgumentBuffer = nil;
}

static void
pl_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;
    
    ptGraphics->sbtRenderPassGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtRenderPassFreeIndices, tHandle.uIndex);

    plMetalRenderPass* ptMetalResource = &ptMetalGraphics->sbtRenderPassesHot[tHandle.uIndex];
    [ptMetalResource->ptRenderPassDescriptor release];
    ptMetalResource->ptRenderPassDescriptor = nil;
}

static void
pl_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;
    
    ptGraphics->sbtRenderPassLayoutGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtRenderPassLayoutFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;
    ptGraphics->sbtShaderGenerations[tHandle.uIndex]++;

    plShader* ptResource = &ptGraphics->sbtShadersCold[tHandle.uIndex];

    for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
    {
        const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
        plMetalShader* ptVariantMetalResource = &ptMetalGraphics->sbtShadersHot[iVariantIndex];
        [ptVariantMetalResource->tDepthStencilState release];
        [ptVariantMetalResource->tRenderPipelineState release];
        ptVariantMetalResource->tDepthStencilState = nil;
        ptVariantMetalResource->tRenderPipelineState = nil;
        pl_sb_push(ptGraphics->sbtShaderFreeIndices, iVariantIndex);
    }
    pl_sb_free(ptResource->_sbtVariantHandles);
    pl_hm_free(&ptResource->tVariantHashmap);
}

static void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;
    ptGraphics->sbtComputeShaderGenerations[tHandle.uIndex]++;

    plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];

    for(uint32_t j = 0; j < pl_sb_size(ptResource->_sbtVariantHandles); j++)
    {
        const uint32_t iVariantIndex = ptResource->_sbtVariantHandles[j].uIndex;
        plMetalComputeShader* ptVariantMetalResource = &ptMetalGraphics->sbtComputeShadersHot[iVariantIndex];
        [ptVariantMetalResource->tPipelineState release];
        ptVariantMetalResource->tPipelineState = nil;
        pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, iVariantIndex);
    }
    pl_sb_free(ptResource->_sbtVariantHandles);
    pl_hm_free(&ptResource->tVariantHashmap);
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
        .dispatch                         = pl_dispatch,
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
        .add_3d_aabb                      = pl__add_3d_aabb,
        .register_3d_drawlist             = pl__register_3d_drawlist,
        .submit_3d_drawlist               = pl__submit_3d_drawlist,
        .get_ui_texture_handle            = pl_get_ui_texture_handle,
        .begin_command_recording          = pl_begin_command_recording,
        .end_command_recording            = pl_end_command_recording,
        .submit_command_buffer            = pl_submit_command_buffer,
        .submit_command_buffer_blocking   = pl_submit_command_buffer_blocking,
        .begin_render_pass                = pl_begin_render_pass,
        .end_render_pass                  = pl_end_render_pass,
        .begin_compute_pass               = pl_begin_compute_pass,
        .end_compute_pass                 = pl_end_compute_pass,
        .begin_blit_pass                  = pl_begin_blit_pass,
        .end_blit_pass                    = pl_end_blit_pass,
        .draw_subpass                     = pl_draw_subpass,
        .present                          = pl_present,
        .copy_buffer_to_texture           = pl_copy_buffer_to_texture,
        .transfer_image_to_buffer         = pl_transfer_image_to_buffer,
        .generate_mipmaps                 = pl_generate_mipmaps,
        .copy_buffer                      = pl_copy_buffer,
        .signal_semaphore                 = pl_signal_semaphore,
        .wait_semaphore                   = pl_wait_semaphore,
        .get_semaphore_value              = pl_get_semaphore_value
    };
    return &tApi;
}

static const plDeviceI*
pl_load_device_api(void)
{
    static const plDeviceI tApi = {
        .create_semaphore                       = pl_create_semaphore,
        .create_buffer                          = pl_create_buffer,
        .create_shader                          = pl_create_shader,
        .create_compute_shader                  = pl_create_compute_shader,
        .create_render_pass_layout              = pl_create_render_pass_layout,
        .create_render_pass                     = pl_create_render_pass,
        .create_texture                         = pl_create_texture,
        .create_texture_view                    = pl_create_texture_view,
        .create_bind_group                      = pl_create_bind_group,
        .get_temporary_bind_group               = pl_get_temporary_bind_group,
        .update_bind_group                      = pl_update_bind_group,
        .allocate_dynamic_data                  = pl_allocate_dynamic_data,
        .queue_buffer_for_deletion              = pl_queue_buffer_for_deletion,
        .queue_texture_for_deletion             = pl_queue_texture_for_deletion,
        .queue_texture_view_for_deletion        = pl_queue_texture_view_for_deletion,
        .queue_bind_group_for_deletion          = pl_queue_bind_group_for_deletion,
        .queue_shader_for_deletion              = pl_queue_shader_for_deletion,
        .queue_compute_shader_for_deletion      = pl_queue_compute_shader_for_deletion,
        .queue_render_pass_for_deletion         = pl_queue_render_pass_for_deletion,
        .queue_render_pass_layout_for_deletion  = pl_queue_render_pass_layout_for_deletion,
        .destroy_texture_view                   = pl_queue_texture_view_for_deletion,
        .destroy_bind_group                     = pl_destroy_bind_group,
        .destroy_buffer                         = pl_destroy_buffer,
        .destroy_texture                        = pl_destroy_texture,
        .destroy_shader                         = pl_destroy_shader,
        .destroy_compute_shader                 = pl_destroy_compute_shader,
        .destroy_render_pass                    = pl_destroy_render_pass,
        .destroy_render_pass_layout             = pl_destroy_render_pass_layout,
        .update_render_pass_attachments         = pl_update_render_pass_attachments,
        .get_buffer                             = pl__get_buffer,
        .get_texture                            = pl__get_texture,
        .get_texture_view                       = pl__get_texture_view,
        .get_bind_group                         = pl__get_bind_group,
        .get_shader                             = pl__get_shader,
        .get_compute_shader_variant             = pl_get_compute_shader_variant,
        .get_shader_variant                     = pl_get_shader_variant
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
    gptOS = ptApiRegistry->first(PL_API_OS_SERVICES);
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
