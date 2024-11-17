/*
   pl_graphics_metal.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs & types
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_profile.h"
#include "pl_memory.h"
#include "pl_graphics_internal.h"

// metal stuff
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

//-----------------------------------------------------------------------------
// [SECTION] internal structs & types
//-----------------------------------------------------------------------------

typedef struct _plCommandBuffer
{
    plCommandPool*       ptPool;
    plBeginCommandInfo   tBeginInfo;
    plDevice*            ptDevice;
    id<MTLCommandBuffer> tCmdBuffer;
    plCommandBuffer*     ptNext;
} plCommandBuffer;

typedef struct _plRenderEncoder
{
    plCommandBuffer*            ptCommandBuffer;
    plRenderPassHandle          tRenderPassHandle;
    uint32_t                    uCurrentSubpass;
    id<MTLRenderCommandEncoder> tEncoder;
    plRenderEncoder*            ptNext;
} plRenderEncoder;

typedef struct _plComputeEncoder
{
    plCommandBuffer*             ptCommandBuffer;
    id<MTLComputeCommandEncoder> tEncoder;
    plComputeEncoder*            ptNext;
} plComputeEncoder;

typedef struct _plBlitEncoder
{
    plCommandBuffer*          ptCommandBuffer;
    id<MTLBlitCommandEncoder> tEncoder;
    plBlitEncoder*            ptNext;
} plBlitEncoder;

typedef struct _plMetalDynamicBuffer
{
    uint32_t                 uHandle;
    plDeviceMemoryAllocation tMemory;
    id<MTLBuffer>            tBuffer;
} plMetalDynamicBuffer;

typedef struct _plMetalRenderPassLayout
{
    int unused;
} plMetalRenderPassLayout;

typedef struct _plMetalFrameBuffer
{
    MTLRenderPassDescriptor** sbptRenderPassDescriptor;
} plMetalFrameBuffer;

typedef struct _plMetalRenderPass
{
    plMetalFrameBuffer atRenderPassDescriptors[PL_MAX_FRAMES_IN_FLIGHT];
    id<MTLFence>       tFence;
    uint32_t           uResolveIndex;
} plMetalRenderPass;

typedef struct _plMetalBuffer
{
    id<MTLBuffer> tBuffer;
    id<MTLHeap>   tHeap;
} plMetalBuffer;

typedef struct _plCommandPool
{
    plDevice*           ptDevice;
    id<MTLCommandQueue> tCmdQueue;
    plCommandBuffer*    ptCommandBufferFreeList;
} plCommandPool;

typedef struct _plBindGroupPool
{
    plDevice*           ptDevice;
    id<MTLHeap>         tDescriptorHeap;
    size_t              szHeapSize;
    plBindGroupPoolDesc tDesc;
    plMetalBuffer       tArgumentBuffer;
    size_t              szCurrentArgumentOffset;
} plBindGroupPool;

typedef struct _plFrameContext
{
    uint32_t              uCurrentBufferIndex;
    plMetalDynamicBuffer* sbtDynamicBuffers;
    dispatch_semaphore_t tFrameBoundarySemaphore;
} plFrameContext;

typedef struct _plMetalTexture
{
    id<MTLTexture> tTexture;
    id<MTLHeap>    tHeap;
    MTLTextureDescriptor* ptTextureDescriptor;
    bool        bOriginalView;
} plMetalTexture;

typedef struct _plMetalSampler
{
    id<MTLSamplerState> tSampler;
} plMetalSampler;

typedef struct _plTimelineSemaphore
{
    plDevice*          ptDevice;
    id<MTLEvent>       tEvent;
    id<MTLSharedEvent> tSharedEvent;
    plTimelineSemaphore* ptNext;
} plTimelineSemaphore;

typedef struct _plMetalBindGroup
{
    id<MTLBuffer> tShaderArgumentBuffer;
    plBindGroupLayout tLayout;
    plTextureHandle atTextureBindings[PL_MAX_TEXTURES_PER_BIND_GROUP];
    plBufferHandle  atBufferBindings[PL_MAX_BUFFERS_PER_BIND_GROUP];
    plSamplerHandle atSamplerBindings[PL_MAX_TEXTURES_PER_BIND_GROUP];
    uint32_t uHeapCount;
    id<MTLHeap> atRequiredHeaps[PL_MAX_TEXTURES_PER_BIND_GROUP * PL_MAX_BUFFERS_PER_BIND_GROUP];
    uint32_t uOffset;
} plMetalBindGroup;

typedef struct _plMetalShader
{
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;
    MTLCullMode                tCullMode;
    MTLTriangleFillMode        tFillMode;
    id<MTLLibrary>             tVertexLibrary;
    id<MTLLibrary>             tFragmentLibrary;
    uint64_t                   ulStencilRef;
} plMetalShader;

typedef struct _plMetalComputeShader
{
    id<MTLComputePipelineState> tPipelineState;
    id<MTLLibrary> library;
} plMetalComputeShader;

typedef struct _plGraphics
{
    plWindow*          ptMainWindow;
    uint32_t           uCurrentFrameIndex;
    uint32_t           uFramesInFlight;
    size_t             szLocalMemoryInUse;
    size_t             szHostMemoryInUse;
    bool               bValidationActive;
    
    // free lists
    plRenderEncoder*  ptRenderEncoderFreeList;
    plBlitEncoder*    ptBlitEncoderFreeList;
    plComputeEncoder* ptComputeEncoderFreeList;

    // metal specifics
    plTempAllocator     tTempAllocator;
    CAMetalLayer*       pMetalLayer;
    id<MTLFence>        tFence;
    
    // per frame
    id<CAMetalDrawable> tCurrentDrawable;
} plGraphics;

typedef struct _plDevice
{
    plDeviceInit              tInit;
    plDeviceInfo              tInfo;
    plFrameGarbage*           sbtGarbage;
    plFrameContext*           sbtFrames;
    bool                      bDescriptorIndexing;
    plDeviceMemoryAllocatorI* ptDynamicAllocator;
    void*                     _pInternalData;

    size_t szDynamicArgumentBufferHeapSize;
    size_t szDynamicArgumentBufferSize;

    plTimelineSemaphore* ptSemaphoreFreeList;

    // render pass layouts
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plMetalRenderPassLayout* sbtRenderPassLayoutsHot;
    plRenderPassLayout*      sbtRenderPassLayoutsCold;
    uint16_t*                sbtRenderPassLayoutFreeIndices;

    // render passes
    plRenderPassHandle tMainRenderPass;
    plMetalRenderPass* sbtRenderPassesHot;
    plRenderPass*      sbtRenderPassesCold;
    uint16_t*          sbtRenderPassFreeIndices;

    // shaders
    plMetalShader* sbtShadersHot;
    plShader*      sbtShadersCold;
    uint16_t*      sbtShaderFreeIndices;

    // compute shaders
    plMetalComputeShader* sbtComputeShadersHot;
    plComputeShader*      sbtComputeShadersCold;
    uint16_t*             sbtComputeShaderFreeIndices;

    // buffers
    plMetalBuffer* sbtBuffersHot;
    plBuffer*      sbtBuffersCold;
    uint16_t*      sbtBufferFreeIndices;

    // textures
    plMetalTexture* sbtTexturesHot;
    plTexture*      sbtTexturesCold;
    uint16_t*       sbtTextureFreeIndices;

    // samplers
    plMetalSampler* sbtSamplersHot;
    plSampler*      sbtSamplersCold;
    uint16_t*       sbtSamplerFreeIndices;

    // bind groups
    plMetalBindGroup*  sbtBindGroupsHot;
    plBindGroup*       sbtBindGroupsCold;
    uint16_t*          sbtBindGroupFreeIndices;

    // metal specifics
    id<MTLDevice> tDevice;
    
} plDevice;

typedef struct _plSwapchain
{
    plDevice*        ptDevice;
    plSwapchainInfo  tInfo;
    uint32_t         uImageCount;
    plTextureHandle* sbtSwapchainTextureViews;
    uint32_t         uCurrentImageIndex; // current image to use within the swap chain
    bool             bVSync;
    plSurface*       ptSurface; // unused
} plSwapchain;

typedef struct _plSurface
{
    int _iUnused;
} plSurface;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// conversion between pilotlight & vulkan types
static MTLSamplerMinMagFilter pl__metal_filter(plFilter tFilter);
static MTLSamplerAddressMode  pl__metal_wrap(plAddressMode tWrap);
static MTLCompareFunction     pl__metal_compare(plCompareMode tCompare);
static MTLStencilOperation    pl__metal_stencil_op(plStencilOp tOp);
static MTLPixelFormat         pl__metal_format(plFormat tFormat);
static MTLVertexFormat        pl__metal_vertex_format(plFormat tFormat);
static MTLCullMode            pl__metal_cull(plCullMode tCullMode);
static MTLLoadAction          pl__metal_load_op   (plLoadOp tOp);
static MTLStoreAction         pl__metal_store_op  (plStoreOp tOp);
static MTLDataType            pl__metal_data_type  (plDataType tType);
static MTLRenderStages        pl__metal_stage_flags(plStageFlags tFlags);
static bool                   pl__is_depth_format  (plFormat tFormat);
static bool                   pl__is_stencil_format  (plFormat tFormat);
static MTLBlendFactor         pl__metal_blend_factor(plBlendFactor tFactor);
static MTLBlendOperation      pl__metal_blend_op(plBlendOp tOp);

static plDeviceMemoryAllocation pl_allocate_memory(plDevice* ptDevice, size_t ulSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName);
static void pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock);

#define PL__ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDesc* ptDesc)
{

    plRenderPassLayoutHandle tHandle = pl__get_new_render_pass_layout_handle(ptDevice);
    plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, tHandle);
    ptLayout->tDesc = *ptDesc;

    ptDevice->sbtRenderPassLayoutsHot[tHandle.uIndex] = (plMetalRenderPassLayout){0};
    return tHandle;
}

static void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{

    plRenderPass* ptRenderPass = pl_get_render_pass(ptDevice, tHandle);
    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    ptRenderPass->tDesc.tDimensions = tDimensions;

    plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, ptRenderPass->tDesc.tLayout);

    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGraphics->uFramesInFlight; uFrameIndex++)
    {
        for(uint32_t i = 0; i < ptLayout->tDesc._uSubpassCount; i++)
        {
            const plSubpass* ptSubpass = &ptLayout->tDesc.atSubpasses[i];

            MTLRenderPassDescriptor* ptRenderPassDescriptor = ptMetalRenderPass->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[i];

            uint32_t uCurrentColorAttachment = 0;
            for(uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
            {
                uint32_t uTargetIndex = ptSubpass->auRenderTargets[j];
                const uint32_t uTextureIndex = ptAttachments[uFrameIndex].atViewAttachments[uTargetIndex].uIndex;
                if(ptLayout->tDesc.atRenderTargets[ptSubpass->auRenderTargets[j]].bDepth)
                {
                    ptRenderPassDescriptor.depthAttachment.texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    ptRenderPassDescriptor.depthAttachment.slice = ptDevice->sbtTexturesCold[uTextureIndex].tView.uBaseLayer;
                    if(pl__is_stencil_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
                    {
                        ptRenderPassDescriptor.stencilAttachment.texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    }
                }
                else if(ptLayout->tDesc.atRenderTargets[ptSubpass->auRenderTargets[j]].bResolve)
                {
                    ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].resolveTexture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                }
                else
                {
                    ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    uCurrentColorAttachment++;
                }
            }
        }
    }
}

static plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDesc* ptDesc, const plRenderPassAttachments* ptAttachments)
{
    plRenderPassHandle tHandle = pl__get_new_render_pass_handle(ptDevice);
    plRenderPass* ptPass = pl_get_render_pass(ptDevice, tHandle);
    ptPass->tDesc = *ptDesc;

    plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, ptDesc->tLayout);

    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    ptMetalRenderPass->tFence = [ptDevice->tDevice newFence];
    ptMetalRenderPass->uResolveIndex = UINT32_MAX;

    ptLayout->tDesc._uSubpassCount = 0;
    for(uint32_t i = 0; i < PL_MAX_SUBPASSES; i++)
    {
        const plSubpass* ptSubpass = &ptLayout->tDesc.atSubpasses[i];

        if(ptSubpass->uRenderTargetCount == 0 && ptSubpass->uSubpassInputCount == 0)
            break;

        ptLayout->tDesc._uSubpassCount++;
    }

    // subpasses
    uint32_t uCount = gptGraphics->uFramesInFlight;
    if(ptDesc->ptSwapchain)
        uCount = ptDesc->ptSwapchain->uImageCount;

    for(uint32_t uFrameIndex = 0; uFrameIndex < uCount; uFrameIndex++)
    {
        bool abTargetSeen[PL_MAX_RENDER_TARGETS] = {0};
        for(uint32_t i = 0; i < PL_MAX_SUBPASSES; i++)
        {
            const plSubpass* ptSubpass = &ptLayout->tDesc.atSubpasses[i];

            if(ptSubpass->uRenderTargetCount == 0 && ptSubpass->uSubpassInputCount == 0)
                break;

            bool bHasResolve = false;
            bool bHasDepth = false;

            MTLRenderPassDescriptor* ptRenderPassDescriptor = [MTLRenderPassDescriptor new];
            // uint32_t auLastFrames[PL_MAX_RENDER_TARGETS] = {0};
            
            uint32_t uCurrentColorAttachment = 0;
            for(uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
            {
                uint32_t uTargetIndex = ptSubpass->auRenderTargets[j];
                const uint32_t uTextureIndex = ptAttachments[uFrameIndex].atViewAttachments[uTargetIndex].uIndex;
                if(ptLayout->tDesc.atRenderTargets[ptSubpass->auRenderTargets[j]].bDepth)
                {
                    bool bStencilIncluded = pl__is_stencil_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat);
                    if(abTargetSeen[uTargetIndex])
                    {
                        ptRenderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
                        if(bStencilIncluded)
                        {
                            ptRenderPassDescriptor.stencilAttachment.loadAction = MTLLoadActionLoad;
                        }
                    }
                    else
                    {
                        
                        ptRenderPassDescriptor.depthAttachment.loadAction = pl__metal_load_op(ptDesc->tDepthTarget.tLoadOp);
                        if(bStencilIncluded)
                        {
                            ptRenderPassDescriptor.stencilAttachment.loadAction = pl__metal_load_op(ptDesc->tDepthTarget.tStencilLoadOp);
                        }
                        abTargetSeen[uTargetIndex] = true;
                    }


                    if(i == ptLayout->tDesc._uSubpassCount - 1)
                    {
                        ptRenderPassDescriptor.depthAttachment.storeAction = pl__metal_store_op(ptDesc->tDepthTarget.tStoreOp);
                        if(bStencilIncluded)
                        {
                            ptRenderPassDescriptor.stencilAttachment.storeAction = pl__metal_store_op(ptDesc->tDepthTarget.tStencilStoreOp);
                        }
                    }
                    else
                    {
                        ptRenderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
                        if(bStencilIncluded)
                        {
                            ptRenderPassDescriptor.stencilAttachment.storeAction = MTLStoreActionStore;
                        }
                    }
                    
                    ptRenderPassDescriptor.depthAttachment.clearDepth = ptDesc->tDepthTarget.fClearZ;

                    if(bStencilIncluded)
                    {
                        ptRenderPassDescriptor.stencilAttachment.clearStencil = ptDesc->tDepthTarget.uClearStencil;
                        ptRenderPassDescriptor.stencilAttachment.texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    }
                    ptRenderPassDescriptor.depthAttachment.texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    ptRenderPassDescriptor.depthAttachment.slice = ptDevice->sbtTexturesCold[uTextureIndex].tView.uBaseLayer;
                    bHasDepth = true;
                }
                else if(ptLayout->tDesc.atRenderTargets[ptSubpass->auRenderTargets[j]].bResolve)
                {
                    ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].resolveTexture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    bHasResolve = true;
                    ptMetalRenderPass->uResolveIndex = uCurrentColorAttachment;
                }
                else
                {
                    const uint32_t uTargetIndexOriginal = uTargetIndex;
                    if(bHasDepth)
                        uTargetIndex--;
                    if(bHasResolve)
                        uTargetIndex--;
                    ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;

                    if(abTargetSeen[uTargetIndexOriginal])
                    {
                        ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].loadAction = MTLLoadActionLoad;
                    }
                    else
                    {
                        ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].loadAction = pl__metal_load_op(ptDesc->atColorTargets[uTargetIndex].tLoadOp);
                        abTargetSeen[uTargetIndexOriginal] = true;
                        
                    }

                    if(i == ptLayout->tDesc._uSubpassCount - 1)
                    {
                        ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].storeAction = pl__metal_store_op(ptDesc->atColorTargets[uTargetIndex].tStoreOp);
                    }
                    else
                    {
                        ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].storeAction = MTLStoreActionStore;
                    }

                    ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].clearColor = MTLClearColorMake(
                        ptDesc->atColorTargets[uTargetIndex].tClearColor.r,
                        ptDesc->atColorTargets[uTargetIndex].tClearColor.g,
                        ptDesc->atColorTargets[uTargetIndex].tClearColor.b,
                        ptDesc->atColorTargets[uTargetIndex].tClearColor.a
                        );
                    uCurrentColorAttachment++;
                }
            }

            pl_sb_push(ptMetalRenderPass->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor, ptRenderPassDescriptor);
        }
    }
    return tHandle;
}

static void
pl_copy_buffer_to_texture(plBlitEncoder* ptEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plMetalBuffer* ptBuffer = &ptDevice->sbtBuffersHot[tBufferHandle.uIndex];
    plMetalTexture* ptTexture = &ptDevice->sbtTexturesHot[tTextureHandle.uIndex];
    plTexture* ptColdTexture = pl__get_texture(ptDevice, tTextureHandle);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {

        MTLOrigin tOrigin;
        tOrigin.x = ptRegions[i].iImageOffsetX;
        tOrigin.y = ptRegions[i].iImageOffsetY;
        tOrigin.z = ptRegions[i].iImageOffsetZ;

        MTLSize tSize;
        tSize.width  = ptRegions[i].uImageWidth;
        tSize.height = ptRegions[i].uImageHeight;
        tSize.depth  = ptRegions[i].uImageDepth;

        NSUInteger uBytesPerRow = tSize.width * pl__format_stride(ptColdTexture->tDesc.tFormat);
        [ptEncoder->tEncoder copyFromBuffer:ptBuffer->tBuffer
            sourceOffset:ptRegions[i].szBufferOffset
            sourceBytesPerRow:uBytesPerRow 
            sourceBytesPerImage:0 
            sourceSize:tSize 
            toTexture:ptTexture->tTexture
            destinationSlice:ptRegions[i].uBaseArrayLayer
            destinationLevel:ptRegions[i].uMipLevel 
            destinationOrigin:tOrigin];
    }
}

static void
pl_copy_texture_to_buffer(plBlitEncoder* ptEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    const plTexture* ptTexture = pl__get_texture(ptDevice, tTextureHandle);
    const plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[tTextureHandle.uIndex];
    const plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[tBufferHandle.uIndex];

    for(uint32_t i = 0; i < uRegionCount; i++)
    {

        MTLOrigin tOrigin;
        tOrigin.x = ptRegions[i].iImageOffsetX;
        tOrigin.y = ptRegions[i].iImageOffsetY;
        tOrigin.z = ptRegions[i].iImageOffsetZ;

        MTLSize tSize;
        tSize.width  = ptRegions[i].uImageWidth;
        tSize.height = ptRegions[i].uImageHeight;
        tSize.depth  = ptRegions[i].uImageDepth;

        const uint32_t uFormatStride = pl__format_stride(ptTexture->tDesc.tFormat);

        [ptEncoder->tEncoder copyFromTexture:ptMetalTexture->tTexture
            sourceSlice:ptRegions[i].uBaseArrayLayer
            sourceLevel:ptRegions[i].uMipLevel
            sourceOrigin:tOrigin
            sourceSize:tSize
            toBuffer:ptMetalBuffer->tBuffer
            destinationOffset:ptRegions[i].szBufferOffset
            destinationBytesPerRow:ptTexture->tDesc.tDimensions.x * uFormatStride
            destinationBytesPerImage:0];
    }
}

static void
pl_copy_buffer(plBlitEncoder* ptEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    [ptEncoder->tEncoder copyFromBuffer:ptDevice->sbtBuffersHot[tSource.uIndex].tBuffer sourceOffset:uSourceOffset toBuffer:ptDevice->sbtBuffersHot[tDestination.uIndex].tBuffer destinationOffset:uDestinationOffset size:szSize];
}

static plTimelineSemaphore*
pl_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plTimelineSemaphore* ptSemaphore = pl__get_new_semaphore(ptDevice);
    
    if(bHostVisible)
    {
        ptSemaphore->tSharedEvent = [ptDevice->tDevice newSharedEvent];
    }
    else
    {
        ptSemaphore->tEvent = [ptDevice->tDevice newEvent];
    }
    return ptSemaphore;
}

static void
pl_cleanup_semaphore(plTimelineSemaphore* ptSemaphore)
{
    pl__return_semaphore(ptSemaphore->ptDevice, ptSemaphore);
}

static void
pl_signal_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
    PL_ASSERT(ptSemaphore->tSharedEvent != nil);
    if(ptSemaphore->tSharedEvent)
    {
        ptSemaphore->tSharedEvent.signaledValue = ulValue;
    }
}

static void
pl_wait_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
    PL_ASSERT(ptSemaphore->tSharedEvent != nil);
    if(ptSemaphore->tSharedEvent)
    {
        while(ptSemaphore->tSharedEvent.signaledValue != ulValue)
        {
            gptThreads->sleep_thread(1);
        }
    }
}

static uint64_t
pl_get_semaphore_value(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore)
{
    PL_ASSERT(ptSemaphore->tSharedEvent != nil);

    if(ptSemaphore->tSharedEvent)
    {
        return ptSemaphore->tSharedEvent.signaledValue;
    }
    return 0;
}

static plBufferHandle
pl_create_buffer(plDevice* ptDevice, const plBufferDesc* ptDesc, plBuffer** ptBufferOut)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);
    plBuffer* ptBuffer = pl__get_buffer(ptDevice, tHandle);

    ptBuffer->tDesc = *ptDesc;

    if(ptDesc->pcDebugName == NULL)
        ptBuffer->tDesc.pcDebugName = "unnamed buffer";

    MTLResourceOptions tStorageMode = MTLResourceStorageModePrivate;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_STAGING)
    {
        tStorageMode = MTLResourceStorageModeShared;
    }

    MTLSizeAndAlign tSizeAndAlign = [ptDevice->tDevice heapBufferSizeAndAlignWithLength:ptDesc->szByteSize options:tStorageMode];
    ptBuffer->tMemoryRequirements.ulSize = tSizeAndAlign.size;
    ptBuffer->tMemoryRequirements.ulAlignment = tSizeAndAlign.align;
    ptBuffer->tMemoryRequirements.uMemoryTypeBits = 0;

    plMetalBuffer tMetalBuffer = {
        0
    };
    ptDevice->sbtBuffersHot[tHandle.uIndex] = tMetalBuffer;
    if(ptBufferOut)
        *ptBufferOut = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    return tHandle;
}

static void
pl_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plBuffer* ptBuffer = pl__get_buffer(ptDevice, tHandle);
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];

    MTLResourceOptions tStorageMode = MTLResourceStorageModeShared;
    if(ptAllocation->tMemoryMode == PL_MEMORY_GPU)
    {
        tStorageMode = MTLResourceStorageModePrivate;
    }

    ptMetalBuffer->tBuffer = [(id<MTLHeap>)ptAllocation->uHandle newBufferWithLength:ptAllocation->ulSize options:tStorageMode offset:ptAllocation->ulOffset];
    ptMetalBuffer->tBuffer.label = [NSString stringWithUTF8String:ptBuffer->tDesc.pcDebugName];

    if(ptAllocation->tMemoryMode != PL_MEMORY_GPU)
    {
        memset(ptMetalBuffer->tBuffer.contents, 0, ptAllocation->ulSize);
        ptBuffer->tMemoryAllocation.pHostMapped = ptMetalBuffer->tBuffer.contents;
    }
    ptMetalBuffer->tHeap = (id<MTLHeap>)ptAllocation->uHandle;
}

static void
pl_generate_mipmaps(plBlitEncoder* ptEncoder, plTextureHandle tTexture)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptTexture = pl__get_texture(ptDevice, tTexture);
    if(ptTexture->tDesc.uMips < 2)
        return;

    [ptEncoder->tEncoder generateMipmapsForTexture:ptDevice->sbtTexturesHot[tTexture.uIndex].tTexture];
}

static plTextureHandle
pl_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, plTexture** ptTextureOut)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    plTexture* ptTexture = pl__get_texture(ptDevice, tHandle);

    plTextureDesc tDesc = *ptDesc;

    if(tDesc.pcDebugName == NULL)
        tDesc.pcDebugName = "unnamed texture";

    if(tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;

    ptTexture->tDesc = tDesc,
    ptTexture->tView = (plTextureViewDesc){
        .tFormat = tDesc.tFormat,
        .uBaseMip = 0,
        .uMips = tDesc.uMips,
        .uBaseLayer = 0,
        .uLayerCount = tDesc.uLayers,
        .tTexture = tHandle
    };

    MTLTextureDescriptor* ptTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    ptTextureDescriptor.pixelFormat = pl__metal_format(tDesc.tFormat);
    ptTextureDescriptor.width = tDesc.tDimensions.x;
    ptTextureDescriptor.height = tDesc.tDimensions.y;
    ptTextureDescriptor.mipmapLevelCount = tDesc.uMips;
    ptTextureDescriptor.arrayLength = tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY ? tDesc.uLayers : 1;
    ptTextureDescriptor.depth = tDesc.tDimensions.z;
    ptTextureDescriptor.sampleCount = ptDesc->tSampleCount == 0 ? 1 : ptDesc->tSampleCount;

    if(tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
        ptTextureDescriptor.usage |= MTLTextureUsageShaderRead;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;
    if(tDesc.tUsage & PL_TEXTURE_USAGE_STORAGE)
        ptTextureDescriptor.usage |= MTLTextureUsageShaderWrite;

    if(tDesc.tType == PL_TEXTURE_TYPE_2D)
        ptTextureDescriptor.textureType = MTLTextureType2D;
    else if(tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        ptTextureDescriptor.textureType = MTLTextureTypeCube;
    else if(tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
        ptTextureDescriptor.textureType = MTLTextureType2DArray;
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    if(ptDesc->tSampleCount > 1)
        ptTextureDescriptor.textureType = MTLTextureType2DMultisample;

    MTLSizeAndAlign tSizeAndAlign = [ptDevice->tDevice heapTextureSizeAndAlignWithDescriptor:ptTextureDescriptor];
    ptTexture->tMemoryRequirements.ulAlignment = tSizeAndAlign.align;
    ptTexture->tMemoryRequirements.ulSize = tSizeAndAlign.size;
    ptTexture->tMemoryRequirements.uMemoryTypeBits = 0;
    plMetalTexture tMetalTexture = {
        .ptTextureDescriptor = ptTextureDescriptor,
        .bOriginalView = true
    };
    ptDevice->sbtTexturesHot[tHandle.uIndex] = tMetalTexture;
    if(ptTextureOut)
        *ptTextureOut = &ptDevice->sbtTexturesCold[tHandle.uIndex];
    return tHandle;
}

static plSamplerHandle
pl_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);
    plSampler* ptSampler = pl_get_sampler(ptDevice, tHandle);
    ptSampler->tDesc = *ptDesc;

    MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
    samplerDesc.minFilter = pl__metal_filter(ptDesc->tMinFilter);
    samplerDesc.magFilter = pl__metal_filter(ptDesc->tMagFilter);
    samplerDesc.mipFilter = ptDesc->tMipmapMode == PL_MIPMAP_MODE_LINEAR ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
    samplerDesc.normalizedCoordinates = ptDesc->bUnnormalizedCoordinates ? NO : YES;
    samplerDesc.supportArgumentBuffers = YES;
    samplerDesc.sAddressMode = pl__metal_wrap(ptDesc->tUAddressMode);
    samplerDesc.tAddressMode = pl__metal_wrap(ptDesc->tVAddressMode);
    samplerDesc.rAddressMode = pl__metal_wrap(ptDesc->tWAddressMode);
    samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack;
    samplerDesc.compareFunction = pl__metal_compare(ptDesc->tCompare);
    samplerDesc.lodMinClamp = ptDesc->fMinMip;
    samplerDesc.lodMaxClamp = ptDesc->fMaxMip;
    // samplerDesc.lodAverage = NO;
    samplerDesc.label = [NSString stringWithUTF8String:ptDesc->pcDebugName];

    if(ptDesc->fMaxAnisotropy == 0.0f)
        samplerDesc.maxAnisotropy = 16.0f;
    else
        samplerDesc.maxAnisotropy = ptDesc->fMaxAnisotropy;

    plMetalSampler tMetalSampler = {
        .tSampler = [ptDevice->tDevice newSamplerStateWithDescriptor:samplerDesc]
    };

    ptDevice->sbtSamplersHot[tHandle.uIndex] = tMetalSampler;
    return tHandle;
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, const plBindGroupDesc* ptDesc)
{
    
    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);
    plBindGroup* ptBindGroup = pl__get_bind_group(ptDevice, tHandle);

    ptBindGroup->tLayout = *ptDesc->ptLayout;
    plBindGroupLayout* ptLayout = &ptBindGroup->tLayout;

    ptLayout->_uBufferBindingCount = 0;
    ptLayout->_uTextureBindingCount = 0;
    ptLayout->_uSamplerBindingCount = 0;

    // count bindings
    for(uint32_t i = 0; i < PL_MAX_TEXTURES_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atTextureBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uTextureBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_BUFFERS_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atBufferBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uBufferBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_SAMPLERS_PER_BIND_GROUP; i++)
    {
        if(ptLayout->atSamplerBindings[i].tStages == PL_STAGE_NONE)
            break;
        ptLayout->_uSamplerBindingCount++;
    }
    
    uint32_t uDescriptorCount = ptLayout->_uTextureBindingCount + ptLayout->_uBufferBindingCount + ptLayout->_uSamplerBindingCount;

    for(uint32_t i = 0; i < ptLayout->_uTextureBindingCount; i++)
    {
        uint32_t uCurrentDescriptorCount = ptLayout->atTextureBindings[i].uDescriptorCount;
        if(uCurrentDescriptorCount== 0)
            uCurrentDescriptorCount = 1;
        if(uCurrentDescriptorCount > 1)
            uDescriptorCount += ptLayout->atTextureBindings[i].uDescriptorCount - 1;
    }

    NSUInteger argumentBufferLength = sizeof(uint64_t) * uDescriptorCount;
    MTLSizeAndAlign tSizeAlign = [ptDevice->tDevice heapBufferSizeAndAlignWithLength:argumentBufferLength options:MTLResourceStorageModeShared];

    plMetalBindGroup tMetalBindGroup = {0};

    tMetalBindGroup.tShaderArgumentBuffer = ptDesc->ptPool->tArgumentBuffer.tBuffer;
    tMetalBindGroup.uOffset = ptDesc->ptPool->szCurrentArgumentOffset;
    ptDesc->ptPool->szCurrentArgumentOffset += argumentBufferLength;
    [tMetalBindGroup.tShaderArgumentBuffer retain];
    if(ptDesc->pcDebugName)
        tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:ptDesc->pcDebugName];
    else
        tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:"unnamed bind group"];
    ptDevice->sbtBindGroupsHot[tHandle.uIndex] = tMetalBindGroup;
    return tHandle;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{

    plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    plBindGroup* ptBindGroup = pl__get_bind_group(ptDevice, tHandle);
    ptMetalBindGroup->uHeapCount = 0;
    const char* pcDescriptorStart = ptMetalBindGroup->tShaderArgumentBuffer.contents;

    uint64_t* pulDescriptorStart = (uint64_t*)&pcDescriptorStart[ptMetalBindGroup->uOffset];

    for(uint32_t i = 0; i < ptData->uBufferCount; i++)
    {
        const plBindGroupUpdateBufferData* ptUpdate = &ptData->atBufferBindings[i];
        plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[ptUpdate->tBuffer.uIndex];
        uint64_t* ppfDestination = &pulDescriptorStart[ptUpdate->uSlot];
        *ppfDestination = ptMetalBuffer->tBuffer.gpuAddress;
        ptMetalBindGroup->atBufferBindings[i] = ptUpdate->tBuffer;

        bool bHeapFound = false;
        for(uint32_t j = 0; j < ptMetalBindGroup->uHeapCount; j++)
        {
            if(ptMetalBindGroup->atRequiredHeaps[j] == ptMetalBuffer->tHeap)
            {
                bHeapFound = true;
                break;
            }
        }

        if(!bHeapFound)
        {
            ptMetalBindGroup->atRequiredHeaps[ptMetalBindGroup->uHeapCount] = ptMetalBuffer->tHeap;
            ptMetalBindGroup->uHeapCount++;
        }
    }

    for(uint32_t i = 0; i < ptData->uTextureCount; i++)
    {
        const plBindGroupUpdateTextureData* ptUpdate = &ptData->atTextureBindings[i];
        plTexture* ptTexture = pl__get_texture(ptDevice, ptUpdate->tTexture);
        plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[ptUpdate->tTexture.uIndex];
        MTLResourceID* pptDestination = (MTLResourceID*)&pulDescriptorStart[ptUpdate->uSlot + ptUpdate->uIndex];
        *pptDestination = ptMetalTexture->tTexture.gpuResourceID;
        ptMetalBindGroup->atTextureBindings[i] = ptUpdate->tTexture;

        bool bHeapFound = false;
        for(uint32_t j = 0; j < ptMetalBindGroup->uHeapCount; j++)
        {
            if(ptMetalBindGroup->atRequiredHeaps[j] == ptMetalTexture->tHeap)
            {
                bHeapFound = true;
                break;
            }
        }

        if(!bHeapFound)
        {
            ptMetalBindGroup->atRequiredHeaps[ptMetalBindGroup->uHeapCount] = ptMetalTexture->tHeap;
            ptMetalBindGroup->uHeapCount++;
        }
    }

    for(uint32_t i = 0; i < ptData->uSamplerCount; i++)
    {
        const plBindGroupUpdateSamplerData* ptUpdate = &ptData->atSamplerBindings[i];
        plMetalSampler* ptMetalSampler = &ptDevice->sbtSamplersHot[ptUpdate->tSampler.uIndex];
        MTLResourceID* pptDestination = (MTLResourceID*)&pulDescriptorStart[ptUpdate->uSlot];
        *pptDestination = ptMetalSampler->tSampler.gpuResourceID;
        ptMetalBindGroup->atSamplerBindings[i] = ptUpdate->tSampler;
    }
}

static plBindGroupPool*
pl_create_bind_group_pool(plDevice* ptDevice, const plBindGroupPoolDesc* ptDesc)
{
    plBindGroupPool* ptPool = PL_ALLOC(sizeof(plBindGroupPool));
    memset(ptPool, 0, sizeof(plBindGroupPool));
    ptPool->tDesc = *ptDesc;

    const size_t szMaxSets =
        ptPool->tDesc.szSamplerBindings + 
        ptPool->tDesc.szUniformBufferBindings +
        ptPool->tDesc.szStorageBufferBindings +
        ptPool->tDesc.szSampledTextureBindings +
        ptPool->tDesc.szStorageTextureBindings +
        ptPool->tDesc.szAttachmentTextureBindings;

    ptPool->szHeapSize = szMaxSets * sizeof(uint32_t);

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = MTLStorageModeShared;
    ptHeapDescriptor.size        = ptPool->szHeapSize;
    ptHeapDescriptor.type        = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
    ptPool->tDescriptorHeap = [ptDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    ptPool->ptDevice = ptDevice;

    plMetalBuffer tArgumentBuffer = {
        .tBuffer = [ptPool->tDescriptorHeap newBufferWithLength:ptPool->szHeapSize options:MTLResourceStorageModeShared offset:0]
    };

    ptPool->tArgumentBuffer = tArgumentBuffer;
    return ptPool;
}

static void
pl_reset_bind_group_pool(plBindGroupPool* ptPool)
{
    ptPool->szCurrentArgumentOffset = 0;
}

static void
pl_cleanup_bind_group_pool(plBindGroupPool* ptPool)
{
    PL_FREE(ptPool);
}

static void
pl_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plTexture* ptTexture = pl__get_texture(ptDevice, tHandle);
    ptTexture->tMemoryAllocation = *ptAllocation;
    plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];

    MTLStorageMode tStorageMode = MTLStorageModeShared;
    if(ptAllocation->tMemoryMode == PL_MEMORY_GPU)
    {
        tStorageMode = MTLStorageModePrivate;
    }
    ptMetalTexture->ptTextureDescriptor.storageMode = tStorageMode;

    ptMetalTexture->tTexture = [(id<MTLHeap>)ptAllocation->uHandle newTextureWithDescriptor:ptMetalTexture->ptTextureDescriptor offset:ptAllocation->ulOffset];
    ptMetalTexture->tHeap = (id<MTLHeap>)ptAllocation->uHandle;
    ptMetalTexture->tTexture.label = [NSString stringWithUTF8String:ptTexture->tDesc.pcDebugName];
    [ptMetalTexture->ptTextureDescriptor release];
    ptMetalTexture->ptTextureDescriptor = nil;
}

static plTextureHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    plTexture* ptNewTexture = pl__get_texture(ptDevice, tHandle);
    plTexture* ptOriginalTexture = pl__get_texture(ptDevice, ptViewDesc->tTexture);

    ptNewTexture->tDesc = ptOriginalTexture->tDesc;
    ptNewTexture->tView = *ptViewDesc;
    ptNewTexture->tDesc.uMips = ptViewDesc->uMips;
    ptNewTexture->tDesc.uLayers = ptViewDesc->uLayerCount;
    ptNewTexture->tView.uBaseMip = 0;
    ptNewTexture->tView.uBaseLayer = 0;

    plMetalTexture* ptOldMetalTexture = &ptDevice->sbtTexturesHot[ptViewDesc->tTexture.uIndex];

    plMetalTexture* ptNewMetalTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];
    ptNewMetalTexture->bOriginalView = false;

    MTLTextureType tTextureType = MTLTextureType2D;

    if(ptNewTexture->tDesc.tType == PL_TEXTURE_TYPE_2D)
        tTextureType = MTLTextureType2D;
    else if(ptNewTexture->tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tTextureType = MTLTextureTypeCube;
    else if(ptNewTexture->tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
    {
        if(ptViewDesc->uLayerCount == 1)
        {
            tTextureType = MTLTextureType2D;
        }
        else
        {
            tTextureType = MTLTextureType2DArray;
        }
        
    }
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    NSRange tLevelRange = {
        .length = ptViewDesc->uMips == 0 ? ptOriginalTexture->tDesc.uMips - ptViewDesc->uBaseMip : ptViewDesc->uMips,
        .location = ptViewDesc->uBaseMip
    };

    NSRange tSliceRange = {
        .length = ptViewDesc->uLayerCount,
        .location = ptViewDesc->uBaseLayer
    };

    // NSRange tLevelRange = {
    //     .length = ptViewDesc->uMips,
    //     .location = ptViewDesc->uBaseMip
    // };

    ptNewMetalTexture->tTexture = [ptOldMetalTexture->tTexture newTextureViewWithPixelFormat:pl__metal_format(ptViewDesc->tFormat) 
            textureType:tTextureType
            levels:tLevelRange
            slices:tSliceRange];

    ptNewMetalTexture->tTexture.label = [NSString stringWithUTF8String:ptViewDesc->pcDebugName];
    ptNewMetalTexture->tHeap = ptOldMetalTexture->tHeap;
    return tHandle;
}

static plDynamicDataBlock
pl_allocate_dynamic_data_block(plDevice* ptDevice)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);


    plMetalDynamicBuffer* ptDynamicBuffer = NULL;

    // first call this frame
    if(ptFrame->uCurrentBufferIndex != 0)
    {
        if(pl_sb_size(ptFrame->sbtDynamicBuffers) <= ptFrame->uCurrentBufferIndex)
        {
            pl_sb_add(ptFrame->sbtDynamicBuffers);
            ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
            static char atNameBuffer[64] = {0};
            pl_sprintf(atNameBuffer, "D-BUF-F%d-%d", (int)gptGraphics->uCurrentFrameIndex, (int)ptFrame->uCurrentBufferIndex);

            ptDynamicBuffer->tMemory = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, 0, ptDevice->tInit.szDynamicBufferBlockSize, 0, atNameBuffer);
            ptDynamicBuffer->tBuffer = [(id<MTLHeap>)ptDynamicBuffer->tMemory.uHandle newBufferWithLength:ptDevice->tInit.szDynamicBufferBlockSize options:MTLResourceStorageModeShared offset:0];
            ptDynamicBuffer->tBuffer.label = [NSString stringWithUTF8String:"buddy allocator"];
            gptGraphics->szHostMemoryInUse += ptDevice->tInit.szDynamicBufferBlockSize;
        }
    }
    
    if(ptDynamicBuffer == NULL)
        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];

    plDynamicDataBlock tBlock = {
        ._uBufferHandle  = ptFrame->uCurrentBufferIndex,
        ._uByteSize      = ptDevice->tInit.szDynamicBufferBlockSize,
        ._pcData         = ptDynamicBuffer->tBuffer.contents,
        ._uAlignment     = 256,
        ._uBumpAmount    = ptDevice->tInit.szDynamicDataMaxSize,
        ._uCurrentOffset = 0
    };
    ptFrame->uCurrentBufferIndex++;
    return tBlock;
}

static plComputeShaderHandle
pl_create_compute_shader(plDevice* ptDevice, const plComputeShaderDesc* ptDescription)
{
    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);
    plComputeShader* ptShader = pl__get_compute_shader(ptDevice, tHandle);
    ptShader->tDesc = *ptDescription;
    ptShader->tDesc._uBindGroupLayoutCount = 0;
    ptShader->tDesc._uConstantCount = 0;

    plMetalComputeShader* ptMetalShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];

    if(ptShader->tDesc.tShader.pcEntryFunc == NULL)
        ptShader->tDesc.tShader.pcEntryFunc = "kernel_main";

    NSString* entryFunc = [NSString stringWithUTF8String:"kernel_main"];

    // compile shader source
    NSError* error = nil;
    NSString* shaderSource = [NSString stringWithUTF8String:(const char*)ptShader->tDesc.tShader.puCode];
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];
    ptMetalShader->library = [ptDevice->tDevice  newLibraryWithSource:shaderSource options:ptCompileOptions error:&error];
    if (ptMetalShader->library == nil)
    {
        NSLog(@"Error: failed to create Metal library: %@", error);
    }
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < PL_MAX_SHADER_SPECIALIZATION_CONSTANTS; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        if(ptConstant->tType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
        ptShader->tDesc._uConstantCount++;
    }

    for (uint32_t i = 0; i < 3; i++)
    {
        if(ptShader->tDesc.atBindGroupLayouts[i].atTextureBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atBufferBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atSamplerBindings[0].tStages == PL_STAGE_NONE)
        {
            break;
        }
        ptShader->tDesc._uBindGroupLayoutCount++;
    }

    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptDescription->pTempConstantData;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
    }

    id<MTLFunction> computeFunction = [ptMetalShader->library newFunctionWithName:entryFunc constantValues:ptConstantValues error:&error];

    if (computeFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
    }

    const plMetalComputeShader tMetalShader = {
        .tPipelineState = [ptDevice->tDevice newComputePipelineStateWithFunction:computeFunction error:&error]
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    ptMetalShader->tPipelineState = tMetalShader.tPipelineState;
    return tHandle;
}

static plShaderHandle
pl_create_shader(plDevice* ptDevice, const plShaderDesc* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);
    plShader* ptShader = pl__get_shader(ptDevice, tHandle);
    ptShader->tDesc = *ptDescription;
    ptShader->tDesc._uBindGroupLayoutCount = 0;
    ptShader->tDesc._uConstantCount = 0;

    plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    if(ptShader->tDesc.tPixelShader.pcEntryFunc == NULL)
        ptShader->tDesc.tPixelShader.pcEntryFunc = "fragment_main";

    if(ptShader->tDesc.tVertexShader.pcEntryFunc == NULL)
        ptShader->tDesc.tVertexShader.pcEntryFunc = "vertex_main";

    NSString* vertexEntry = [NSString stringWithUTF8String:"vertex_main"];
    NSString* fragmentEntry = [NSString stringWithUTF8String:"fragment_main"];

    // vertex layout
    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.layouts[4].stepRate = 1;
    vertexDescriptor.layouts[4].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[4].stride = ptDescription->atVertexBufferLayouts[0].uByteStride;

    uint32_t uCurrentAttributeCount = 0;
    for(uint32_t i = 0; i < PL_MAX_VERTEX_ATTRIBUTES; i++)
    {
        if(ptDescription->atVertexBufferLayouts[0].atAttributes[i].tFormat == PL_FORMAT_UNKNOWN)
            break;
        vertexDescriptor.attributes[i].bufferIndex = 4;
        vertexDescriptor.attributes[i].offset = ptDescription->atVertexBufferLayouts[0].atAttributes[i].uByteOffset;
        vertexDescriptor.attributes[i].format = pl__metal_vertex_format(ptDescription->atVertexBufferLayouts[0].atAttributes[i].tFormat);
        uCurrentAttributeCount++;
    }

    // prepare preprocessor defines
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];
    ptCompileOptions.fastMathEnabled = false;

    // compile shader source
    NSError* error = nil;
    NSString* vertexSource = [NSString stringWithUTF8String:(const char*)ptShader->tDesc.tVertexShader.puCode];
    ptMetalShader->tVertexLibrary = [ptDevice->tDevice  newLibraryWithSource:vertexSource options:ptCompileOptions error:&error];
    if (ptMetalShader->tVertexLibrary == nil)
    {
        NSLog(@"Error: failed to create Metal vertex library: %@", error);
    }

    if(ptShader->tDesc.tPixelShader.puCode)
    {
        NSString* fragmentSource = [NSString stringWithUTF8String:(const char*)ptShader->tDesc.tPixelShader.puCode];
        ptMetalShader->tFragmentLibrary = [ptDevice->tDevice  newLibraryWithSource:fragmentSource options:ptCompileOptions error:&error];
        if (ptMetalShader->tFragmentLibrary == nil)
        {
            NSLog(@"Error: failed to create Metal fragment library: %@", error);
        }
    }

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    // renderpass stuff
    const plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, ptShader->tDesc.tRenderPassLayout);

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < PL_MAX_SHADER_SPECIALIZATION_CONSTANTS; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        if(ptConstant->tType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
        ptShader->tDesc._uConstantCount++;
    }

    for (uint32_t i = 0; i < 3; i++)
    {
        if(ptShader->tDesc.atBindGroupLayouts[i].atTextureBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atBufferBindings[0].tStages == PL_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atSamplerBindings[0].tStages == PL_STAGE_NONE)
        {
            break;
        }
        ptShader->tDesc._uBindGroupLayoutCount++;
    }

    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptDescription->pTempConstantData;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
    }

    id<MTLFunction> vertexFunction = [ptMetalShader->tVertexLibrary newFunctionWithName:vertexEntry constantValues:ptConstantValues error:&error];
    id<MTLFunction> fragmentFunction = nil;

    if (vertexFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
    }

    if(ptShader->tDesc.tPixelShader.puCode)
    {
        fragmentFunction = [ptMetalShader->tFragmentLibrary newFunctionWithName:fragmentEntry constantValues:ptConstantValues error:&error];
        if (fragmentFunction == nil)
        {
            NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        }
    }

    MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
    depthDescriptor.depthCompareFunction = pl__metal_compare((plCompareMode)ptDescription->tGraphicsState.ulDepthMode);
    depthDescriptor.depthWriteEnabled = ptDescription->tGraphicsState.ulDepthWriteEnabled ? YES : NO;

    if(ptDescription->tGraphicsState.ulStencilTestEnabled)
    {
        MTLStencilDescriptor* ptStencilDescriptor = [MTLStencilDescriptor new];
        ptStencilDescriptor.readMask = (uint32_t)ptDescription->tGraphicsState.ulStencilMask;
        ptStencilDescriptor.writeMask = (uint32_t)ptDescription->tGraphicsState.ulStencilMask;
        ptStencilDescriptor.stencilCompareFunction    = pl__metal_compare((plCompareMode)ptDescription->tGraphicsState.ulStencilMode);
        ptStencilDescriptor.stencilFailureOperation   = pl__metal_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpFail),
        ptStencilDescriptor.depthFailureOperation     = pl__metal_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpDepthFail),
        ptStencilDescriptor.depthStencilPassOperation = pl__metal_stencil_op((plStencilOp)ptDescription->tGraphicsState.ulStencilOpPass),
        depthDescriptor.backFaceStencil = ptStencilDescriptor;
        depthDescriptor.frontFaceStencil = ptStencilDescriptor;
    }
    ptMetalShader->ulStencilRef = ptDescription->tGraphicsState.ulStencilRef;

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.rasterSampleCount = ptDescription->tMSAASampleCount == 0 ? 1 : ptDescription->tMSAASampleCount;

    uint32_t uCurrentColorAttachment = 0;
    const plSubpass* ptSubpass = &ptLayout->tDesc.atSubpasses[ptDescription->uSubpassIndex];
    for(uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
    {
        const uint32_t uTargetIndex = ptSubpass->auRenderTargets[j];
        if(ptLayout->tDesc.atRenderTargets[uTargetIndex].bDepth)
        {
            pipelineDescriptor.depthAttachmentPixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat);
            if(pl__is_stencil_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
            {
                pipelineDescriptor.stencilAttachmentPixelFormat = pipelineDescriptor.depthAttachmentPixelFormat;
            }
        }
        else if(ptLayout->tDesc.atRenderTargets[uTargetIndex].bResolve)
        {
        }
        else
        {
            pipelineDescriptor.colorAttachments[uCurrentColorAttachment].pixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat);
            pipelineDescriptor.colorAttachments[uCurrentColorAttachment].blendingEnabled = ptDescription->atBlendStates[uCurrentColorAttachment].bBlendEnabled ? YES : NO;
  
            if(ptDescription->atBlendStates[uCurrentColorAttachment].bBlendEnabled)
            {
                pipelineDescriptor.colorAttachments[uCurrentColorAttachment].sourceRGBBlendFactor        = pl__metal_blend_factor(ptDescription->atBlendStates[uCurrentColorAttachment].tSrcColorFactor);
                pipelineDescriptor.colorAttachments[uCurrentColorAttachment].destinationRGBBlendFactor   = pl__metal_blend_factor(ptDescription->atBlendStates[uCurrentColorAttachment].tDstColorFactor);
                pipelineDescriptor.colorAttachments[uCurrentColorAttachment].rgbBlendOperation           = pl__metal_blend_op(ptDescription->atBlendStates[uCurrentColorAttachment].tColorOp);
                pipelineDescriptor.colorAttachments[uCurrentColorAttachment].sourceAlphaBlendFactor      = pl__metal_blend_factor(ptDescription->atBlendStates[uCurrentColorAttachment].tSrcAlphaFactor);
                pipelineDescriptor.colorAttachments[uCurrentColorAttachment].destinationAlphaBlendFactor = pl__metal_blend_factor(ptDescription->atBlendStates[uCurrentColorAttachment].tDstAlphaFactor);
                pipelineDescriptor.colorAttachments[uCurrentColorAttachment].alphaBlendOperation         = pl__metal_blend_op(ptDescription->atBlendStates[uCurrentColorAttachment].tAlphaOp);
            }
            uCurrentColorAttachment++;
        }
    }

    const plMetalShader tMetalShader = {
        .tDepthStencilState   = [ptDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor],
        .tRenderPipelineState = [ptDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error],
        .tCullMode            = pl__metal_cull(ptDescription->tGraphicsState.ulCullMode)
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);
    
    ptMetalShader->tDepthStencilState = tMetalShader.tDepthStencilState;
    ptMetalShader->tRenderPipelineState = tMetalShader.tRenderPipelineState;
    ptMetalShader->tCullMode = tMetalShader.tCullMode;

    return tHandle;
}

typedef struct _plInternalDeviceAllocatorData
{
    plDevice* ptDevice;
    plDeviceMemoryAllocatorI* ptAllocator;
} plInternalDeviceAllocatorData;

static plDeviceMemoryAllocation
pl_allocate_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped = NULL,
        .uHandle     = 0,
        .ulOffset    = 0,
        .ulSize      = ulSize,
        .ptAllocator = ptData->ptAllocator,
        .tMemoryMode = PL_MEMORY_GPU_CPU
    };


    plDeviceMemoryAllocation tBlock = pl_allocate_memory(ptData->ptDevice, ulSize, PL_MEMORY_GPU_CPU, uTypeFilter, "Uncached Heap");
    tAllocation.uHandle = tBlock.uHandle;
    tAllocation.pHostMapped = tBlock.pHostMapped;
    gptGraphics->szHostMemoryInUse += ulSize;
    return tAllocation;
}

static void
pl_free_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;
    plDeviceMemoryAllocation tBlock = {.uHandle = ptAllocation->uHandle};
    pl_free_memory(ptData->ptDevice, &tBlock);
    gptGraphics->szHostMemoryInUse -= ptAllocation->ulSize;
    ptAllocation->uHandle = 0;
    ptAllocation->ulSize = 0;
    ptAllocation->ulOffset = 0;
}

static bool
pl_initialize_graphics(const plGraphicsInit* ptDesc)
{
    static plGraphics gtGraphics = {0};
    gptGraphics = &gtGraphics;
    gptDataRegistry->set_data("plGraphics", gptGraphics);
    gptGraphics->bValidationActive = ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);

    plLogChannelInit tLogInit = {
        .tType       = PL_CHANNEL_TYPE_CYCLIC_BUFFER,
        .uEntryCount = 256
    };
    uLogChannelGraphics = pl_add_log_channel("Graphics", tLogInit);
    uint32_t uLogLevel = PL_LOG_LEVEL_INFO;

    pl_set_log_level(uLogChannelGraphics, uLogLevel);

    return true;
}

static plSurface*
pl_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));
    return ptSurface;
}

static void
pl_enumerate_devices(plDeviceInfo* atDeviceInfo, uint32_t* puDeviceCount)
{
    *puDeviceCount = 1;

    if(atDeviceInfo == NULL)
        return;

    plIO* ptIOCtx = gptIOI->get_io();
    id<MTLDevice> tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    strncpy(atDeviceInfo[0].acName, [tDevice.name UTF8String], 256);
    atDeviceInfo[0].tVendorId = PL_VENDOR_ID_APPLE;

    // printf("%s\n", [tDevice.architecture.name UTF8String]);
    atDeviceInfo[0].tType = PL_DEVICE_TYPE_INTEGRATED;
    atDeviceInfo[0].tCapabilities = PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING | PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY | PL_DEVICE_CAPABILITY_SWAPCHAIN;

    if(tDevice.hasUnifiedMemory)
    {
        atDeviceInfo[0].szHostMemory = tDevice.recommendedMaxWorkingSetSize;
    }
    else
    {
        atDeviceInfo[0].szDeviceMemory = tDevice.recommendedMaxWorkingSetSize;
    }

    if([tDevice supportsTextureSampleCount:64])      atDeviceInfo[0].tMaxSampleCount = 64;
    else if([tDevice supportsTextureSampleCount:32]) atDeviceInfo[0].tMaxSampleCount = 32;
    else if([tDevice supportsTextureSampleCount:16]) atDeviceInfo[0].tMaxSampleCount = 16;
    else if([tDevice supportsTextureSampleCount:8])  atDeviceInfo[0].tMaxSampleCount = 8;
    else if([tDevice supportsTextureSampleCount:4])  atDeviceInfo[0].tMaxSampleCount = 4;
    else if([tDevice supportsTextureSampleCount:2])  atDeviceInfo[0].tMaxSampleCount = 2;
    else atDeviceInfo[0].tMaxSampleCount = 1;
    
}

static plDevice*
pl_create_device(const plDeviceInit* ptInit)
{
    plIO* ptIOCtx = gptIOI->get_io();

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));
    ptDevice->tInit = *ptInit;

    pl_sb_add(ptDevice->sbtRenderPassLayoutsHot);
    pl_sb_add(ptDevice->sbtRenderPassesHot);
    pl_sb_add(ptDevice->sbtShadersHot);
    pl_sb_add(ptDevice->sbtComputeShadersHot);
    pl_sb_add(ptDevice->sbtBuffersHot);
    pl_sb_add(ptDevice->sbtTexturesHot);
    pl_sb_add(ptDevice->sbtSamplersHot);
    pl_sb_add(ptDevice->sbtBindGroupsHot);
    
    pl_sb_add(ptDevice->sbtRenderPassLayoutsCold);
    pl_sb_add(ptDevice->sbtShadersCold);
    pl_sb_add(ptDevice->sbtComputeShadersCold);
    pl_sb_add(ptDevice->sbtBuffersCold);
    pl_sb_add(ptDevice->sbtTexturesCold);
    pl_sb_add(ptDevice->sbtSamplersCold);
    pl_sb_add(ptDevice->sbtBindGroupsCold);

    pl_sb_back(ptDevice->sbtRenderPassLayoutsCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtComputeShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBuffersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtTexturesCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtSamplersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBindGroupsCold)._uGeneration = 1;

    ptDevice->tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    pl_enumerate_devices(atDeviceInfos, &uDeviceCount);

    memcpy(&ptDevice->tInfo, &atDeviceInfos[ptInit->uDeviceIdx], sizeof(plDeviceInfo));

    if(ptDevice->tInit.szDynamicBufferBlockSize == 0) ptDevice->tInit.szDynamicBufferBlockSize = 134217728;
    if(ptDevice->tInit.szDynamicDataMaxSize == 0)     ptDevice->tInit.szDynamicDataMaxSize = 256;


    const size_t szMaxDynamicBufferDescriptors = ptDevice->tInit.szDynamicBufferBlockSize / ptDevice->tInit.szDynamicDataMaxSize;

    ptDevice->szDynamicArgumentBufferHeapSize = sizeof(uint64_t) * szMaxDynamicBufferDescriptors;
    ptDevice->szDynamicArgumentBufferSize = sizeof(uint64_t) * 256;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~device memory allocators~~~~~~~~~~~~~~~~~~~~~~~~~

    static plInternalDeviceAllocatorData tAllocatorData = {0};
    static plDeviceMemoryAllocatorI tAllocator = {0};
    tAllocatorData.ptAllocator = &tAllocator;
    tAllocatorData.ptDevice = ptDevice;
    tAllocator.allocate = pl_allocate_staging_dynamic;
    tAllocator.free = pl_free_staging_dynamic;
    tAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tAllocatorData;
    ptDevice->ptDynamicAllocator = &tAllocator;
    plDeviceMemoryAllocatorI* ptDynamicAllocator = &tAllocator;

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = MTLStorageModeShared;
    ptHeapDescriptor.size        = ptDevice->szDynamicArgumentBufferHeapSize;
    ptHeapDescriptor.type        = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
    // ptHeapDescriptor.sparsePageSize = MTLSparsePageSize256;

    pl_sb_resize(ptDevice->sbtGarbage, gptGraphics->uFramesInFlight + 1);
    gptGraphics->tFence = [ptDevice->tDevice newFence];
    plTempAllocator tTempAllocator = {0};
    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {
            .tFrameBoundarySemaphore = dispatch_semaphore_create(1),
        };
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        static char atNameBuffer[PL_MAX_NAME_LENGTH] = {0};
        pl_sprintf(atNameBuffer, "D-BUF-F%d-0", (int)i);
        tFrame.sbtDynamicBuffers[0].tMemory = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, 0, ptDevice->tInit.szDynamicBufferBlockSize, 0,atNameBuffer);
        tFrame.sbtDynamicBuffers[0].tBuffer = [(id<MTLHeap>)tFrame.sbtDynamicBuffers[0].tMemory.uHandle newBufferWithLength:ptDevice->tInit.szDynamicBufferBlockSize options:MTLResourceStorageModeShared offset:0];
        tFrame.sbtDynamicBuffers[0].tBuffer.label = [NSString stringWithUTF8String:pl_temp_allocator_sprintf(&tTempAllocator, "Dynamic Buffer: %u, 0", i)];
        pl_sb_push(ptDevice->sbtFrames, tFrame);
    }
    pl_temp_allocator_free(&tTempAllocator);

    return ptDevice;
}

static plSwapchain*
pl_create_swapchain(plDevice* ptDevice, plSurface* ptSurface, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));

    ptSwap->tInfo.bVSync = ptInit->bVSync;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->ptSurface = ptSurface;
    ptSwap->ptDevice = ptDevice;
    ptSwap->uImageCount = gptGraphics->uFramesInFlight;
    ptSwap->tInfo.tFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptSwap->bVSync = true;

    ptSwap->tInfo.tSampleCount = pl_min(ptInit->tSampleCount, ptSwap->ptDevice->tInfo.tMaxSampleCount);
    if(ptSwap->tInfo.tSampleCount == 0)
        ptSwap->tInfo.tSampleCount = 1;


    for(uint32_t i = 0; i < ptSwap->uImageCount; i++)
    {
        plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
        plTexture* ptTexture = pl__get_texture(ptDevice, tHandle);
        pl_sb_push(ptSwap->sbtSwapchainTextureViews, tHandle);
        ptTexture->tDesc.tDimensions = (plVec3){gptIO->tMainViewportSize.x, gptIO->tMainViewportSize.y, 1.0f};
        ptTexture->tDesc.uLayers = 1;
        ptTexture->tDesc.uMips = 1;
        ptTexture->tDesc.tSampleCount = ptSwap->tInfo.tSampleCount;
        ptTexture->tDesc.tFormat = ptSwap->tInfo.tFormat;
        ptTexture->tDesc.tType = PL_TEXTURE_TYPE_2D;
        ptTexture->tDesc.tUsage = PL_TEXTURE_USAGE_PRESENT;
        ptTexture->tDesc.pcDebugName = "swapchain dummy image";
        ptTexture->tView.tFormat = ptTexture->tDesc.tFormat;
        ptTexture->tView.uBaseMip = 0;
        ptTexture->tView.uBaseLayer = 0;
        ptTexture->tView.uMips = 1;
        ptTexture->tView.uLayerCount = 1;
        ptTexture->tView.tTexture = tHandle;
        ptTexture->tView.pcDebugName = "swapchain dummy image view";
    }
    ptSwap->tInfo.uWidth = (uint32_t)gptIO->tMainViewportSize.x;
    ptSwap->tInfo.uHeight = (uint32_t)gptIO->tMainViewportSize.y;
    pl_sb_resize(ptSwap->sbtSwapchainTextureViews, gptGraphics->uFramesInFlight);

    return ptSwap;
}

static plTextureHandle*
pl_get_swapchain_images(plSwapchain* ptSwap, uint32_t* puSizeOut)
{
    if(puSizeOut)
        *puSizeOut = ptSwap->uImageCount;
    return ptSwap->sbtSwapchainTextureViews;
}

static void
pl_recreate_swapchain(plSwapchain* ptSwap, const plSwapchainInit* ptInit)
{
    gptGraphics->uCurrentFrameIndex = 0;
    ptSwap->tInfo.bVSync = ptInit->bVSync;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->tInfo.tSampleCount = pl_min(ptInit->tSampleCount, ptSwap->ptDevice->tInfo.tMaxSampleCount);
    if(ptSwap->tInfo.tSampleCount == 0)
        ptSwap->tInfo.tSampleCount = 1;
}

static plCommandPool*
pl_create_command_pool(plDevice* ptDevice, const plCommandPoolDesc* ptDesc)
{
    plCommandPool* ptPool = PL_ALLOC(sizeof(plCommandPool));
    memset(ptPool, 0, sizeof(plCommandPool));

    ptPool->ptDevice = ptDevice;
    ptPool->tCmdQueue = [ptDevice->tDevice newCommandQueue];
    return ptPool;
}

static void
pl_cleanup_command_pool(plCommandPool* ptPool)
{
    plCommandBuffer* ptCurrentCommandBuffer = ptPool->ptCommandBufferFreeList;
    while(ptCurrentCommandBuffer)
    {
        plCommandBuffer* ptNextCommandBuffer = ptCurrentCommandBuffer->ptNext;
        PL_FREE(ptCurrentCommandBuffer);
        ptCurrentCommandBuffer = ptNextCommandBuffer;
    }
    PL_FREE(ptPool);
}

static void
pl_reset_command_pool(plCommandPool* ptPool, plCommandPoolResetFlags tFlags)
{
}

static void
pl_reset_command_buffer(plCommandBuffer* ptCommandBuffer)
{
    MTLCommandBufferDescriptor* ptCmdBufferDescriptor = [MTLCommandBufferDescriptor new];
    ptCmdBufferDescriptor.retainedReferences = NO;
    ptCmdBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    ptCommandBuffer->tCmdBuffer = [ptCommandBuffer->ptPool->tCmdQueue commandBufferWithDescriptor:ptCmdBufferDescriptor];
}

static plCommandBuffer*
pl_request_command_buffer(plCommandPool* ptPool)
{
    plCommandBuffer* ptCommandBuffer = ptPool->ptCommandBufferFreeList;
    if(ptCommandBuffer)
    {
        ptPool->ptCommandBufferFreeList = ptCommandBuffer->ptNext;
    }
    else
    {
        ptCommandBuffer = PL_ALLOC(sizeof(plCommandBuffer));
        memset(ptCommandBuffer, 0, sizeof(plCommandBuffer));
    }

    MTLCommandBufferDescriptor* ptCmdBufferDescriptor = [MTLCommandBufferDescriptor new];
    ptCmdBufferDescriptor.retainedReferences = NO;
    ptCmdBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    ptCommandBuffer->tCmdBuffer = [ptPool->tCmdQueue commandBufferWithDescriptor:ptCmdBufferDescriptor];
    
    // [ptCmdBufferDescriptor release];
    // char blah[32] = {0};
    // pl_sprintf(blah, "%u", gptGraphics->uCurrentFrameIndex);
    // tCmdBuffer.label = [NSString stringWithUTF8String:blah];

    // [ptCmdBufferDescriptor release];
    ptCommandBuffer->ptDevice = ptPool->ptDevice;
    ptCommandBuffer->ptPool = ptPool;
    return ptCommandBuffer;
}

static void
pl_begin_frame(plDevice* ptDevice)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    // Wait until the inflight command buffer has completed its work
    // gptGraphics->tSwapchain.uCurrentImageIndex = gptGraphics->uCurrentFrameIndex;
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    dispatch_semaphore_wait(ptFrame->tFrameBoundarySemaphore, DISPATCH_TIME_FOREVER);

    static bool bFirstRun = true;
    if(bFirstRun == false)
    {
        pl__garbage_collect(ptDevice);
    }
    else
    {
        bFirstRun = false;
    }
    
    pl_end_profile_sample(0);
}

static bool
pl_acquire_swapchain_image(plSwapchain* ptSwapchain)
{
    pl_begin_profile_sample(0, __FUNCTION__);

    plDevice* ptDevice = ptSwapchain->ptDevice;

    plIO* ptIOCtx = gptIOI->get_io();
    gptGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;

    
    // get next drawable
    gptGraphics->tCurrentDrawable = [gptGraphics->pMetalLayer nextDrawable];

    if(!gptGraphics->tCurrentDrawable)
    {
        pl_end_profile_sample(0);
        return false;
    }

    pl_end_profile_sample(0);
    return true;
}

static void
pl_begin_command_recording(plCommandBuffer* ptCommandBuffer, const plBeginCommandInfo* ptBeginInfo)
{
    if(ptBeginInfo)
    {
        plDevice* ptDevice = ptCommandBuffer->ptDevice;
        ptCommandBuffer->tBeginInfo = *ptBeginInfo;
        for(uint32_t i = 0; i < ptBeginInfo->uWaitSemaphoreCount; i++)
        {
            if(ptBeginInfo->atWaitSempahores[i]->tEvent)
            {
                [ptCommandBuffer->tCmdBuffer encodeWaitForEvent:ptBeginInfo->atWaitSempahores[i]->tEvent value:ptBeginInfo->auWaitSemaphoreValues[i]];
            }
            else
            {
                [ptCommandBuffer->tCmdBuffer encodeWaitForEvent:ptBeginInfo->atWaitSempahores[i]->tSharedEvent value:ptBeginInfo->auWaitSemaphoreValues[i]];
            }
        }
    }
}

static void
pl_end_command_recording(plCommandBuffer* ptCommandBuffer)
{
    [ptCommandBuffer->tCmdBuffer enqueue];
}

static bool
pl_present(plCommandBuffer* ptCommandBuffer, const plSubmitInfo* ptSubmitInfo, plSwapchain** ptSwaps, uint32_t uSwapchainCount)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;

    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    [ptCommandBuffer->tCmdBuffer presentDrawable:gptGraphics->tCurrentDrawable];

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            if(ptSubmitInfo->atSignalSempahores[i]->tEvent)
            {
                [ptCommandBuffer->tCmdBuffer encodeSignalEvent:ptSubmitInfo->atSignalSempahores[i]->tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [ptCommandBuffer->tCmdBuffer encodeSignalEvent:ptSubmitInfo->atSignalSempahores[i]->tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }
    
    ptFrame->uCurrentBufferIndex = 0;

    __block dispatch_semaphore_t semaphore = ptFrame->tFrameBoundarySemaphore;
    [ptCommandBuffer->tCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {

        if(commandBuffer.status == MTLCommandBufferStatusError)
        {
            NSLog(@"PRESENT: %@s", commandBuffer.error);
        }
        // GPU work is complete
        // Signal the semaphore to start the CPU work
        dispatch_semaphore_signal(semaphore);
        
    }];

    [ptCommandBuffer->tCmdBuffer commit];

    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    return true;
}

static void
pl_next_subpass(plRenderEncoder* ptEncoder)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    ptEncoder->uCurrentSubpass++;

    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[ptEncoder->tRenderPassHandle.uIndex];

    [ptEncoder->tEncoder updateFence:ptMetalRenderPass->tFence afterStages:MTLRenderStageFragment | MTLRenderStageVertex];
    [ptEncoder->tEncoder endEncoding];

    id<MTLRenderCommandEncoder> tNewRenderEncoder = [ptCmdBuffer->tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[ptEncoder->uCurrentSubpass]];
    tNewRenderEncoder.label = @"subpass encoder";
    [tNewRenderEncoder waitForFence:ptMetalRenderPass->tFence beforeStages:MTLRenderStageFragment | MTLRenderStageVertex];
    ptEncoder->tEncoder = tNewRenderEncoder;
}

static plRenderEncoder*
pl_begin_render_pass(plCommandBuffer* ptCmdBuffer, plRenderPassHandle tPass)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plRenderEncoder* ptEncoder = pl__get_new_render_encoder();

    plRenderPass* ptRenderPass = pl_get_render_pass(ptDevice, tPass);
    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[tPass.uIndex];
    plRenderPassLayout* ptLayout = pl_get_render_pass_layout(ptDevice, ptRenderPass->tDesc.tLayout);

    if(ptRenderPass->tDesc.ptSwapchain)
    {
        if(ptMetalRenderPass->uResolveIndex == UINT32_MAX)
            ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0].colorAttachments[0].texture = gptGraphics->tCurrentDrawable.texture;
        else
            ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0].colorAttachments[ptMetalRenderPass->uResolveIndex].resolveTexture = gptGraphics->tCurrentDrawable.texture;
        ptEncoder->tEncoder = [ptCmdBuffer->tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0]];
        ptEncoder->tEncoder.label = @"main encoder";
    }
    else
    {
        ptEncoder->tEncoder = [ptCmdBuffer->tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0]];
        ptEncoder->tEncoder.label = @"offscreen encoder";
        [ptEncoder->tEncoder waitForFence:ptMetalRenderPass->tFence beforeStages:MTLRenderStageFragment | MTLRenderStageVertex];
    }

    ptEncoder->ptCommandBuffer = ptCmdBuffer;
    ptEncoder->tRenderPassHandle = tPass;
    ptEncoder->uCurrentSubpass = 0;
    return ptEncoder;
}

static void
pl_end_render_pass(plRenderEncoder* ptEncoder)
{
    plDevice* ptDevice = ptEncoder->ptCommandBuffer->ptDevice;

    plRenderPass* ptRenderPass = pl_get_render_pass(ptDevice, ptEncoder->tRenderPassHandle);
    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[ptEncoder->tRenderPassHandle.uIndex];
    if(ptRenderPass->tDesc.ptSwapchain == NULL)
    {
        [ptEncoder->tEncoder updateFence:ptMetalRenderPass->tFence afterStages:MTLRenderStageFragment | MTLRenderStageVertex];
    }
    [ptEncoder->tEncoder endEncoding];
    pl__return_render_encoder(ptEncoder);
}

static void
pl_submit_command_buffer(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {

            if(ptSubmitInfo->atSignalSempahores[i]->tEvent)
            {
                [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptSubmitInfo->atSignalSempahores[i]->tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptSubmitInfo->atSignalSempahores[i]->tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }

    [ptCmdBuffer->tCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {

        if(commandBuffer.status == MTLCommandBufferStatusError)
        {
            NSLog(@"SECONDARY: %@s", commandBuffer.error);
        }
        
    }];

    [ptCmdBuffer->tCmdBuffer commit];
}

static void
pl_wait_on_command_buffer(plCommandBuffer* ptCmdBuffer)
{
    [ptCmdBuffer->tCmdBuffer waitUntilCompleted];
}

static void
pl_return_command_buffer(plCommandBuffer* ptCmdBuffer)
{
    ptCmdBuffer->ptNext = ptCmdBuffer->ptPool->ptCommandBufferFreeList;
    ptCmdBuffer->ptPool->ptCommandBufferFreeList = ptCmdBuffer;
}

static void
pl_set_texture_usage(plBlitEncoder* ptEncoder, plTextureHandle tHandle, plTextureUsage tNewUsage, plTextureUsage tOldUsage)
{
}

static plBlitEncoder*
pl_begin_blit_pass(plCommandBuffer* ptCmdBuffer)
{
    plBlitEncoder* ptEncoder = pl__get_new_blit_encoder();
    ptEncoder->tEncoder = [ptCmdBuffer->tCmdBuffer blitCommandEncoder];
    // plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    // [tBlitEncoder waitForFence:ptFrame->tFence];
    ptEncoder->ptCommandBuffer = ptCmdBuffer;
    return ptEncoder;
}

static void
pl_end_blit_pass(plBlitEncoder* ptEncoder)
{
    [ptEncoder->tEncoder endEncoding];
    pl__return_blit_encoder(ptEncoder);
}

static plComputeEncoder*
pl_begin_compute_pass(plCommandBuffer* ptCmdBuffer)
{
    plComputeEncoder* ptEncoder = pl__get_new_compute_encoder();
    ptEncoder->tEncoder = [ptCmdBuffer->tCmdBuffer computeCommandEncoder];
    ptEncoder->ptCommandBuffer = ptCmdBuffer;
    return ptEncoder;
}

static void
pl_end_compute_pass(plComputeEncoder* ptEncoder)
{
    [ptEncoder->tEncoder endEncoding];
    pl__return_compute_encoder(ptEncoder);
}

static void
pl_dispatch(plComputeEncoder* ptEncoder, uint32_t uDispatchCount, const plDispatch* atDispatches)
{
    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        MTLSize tGridSize = MTLSizeMake(ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
        MTLSize tThreadsPerGroup = MTLSizeMake(ptDispatch->uThreadPerGroupX, ptDispatch->uThreadPerGroupY, ptDispatch->uThreadPerGroupZ);
        [ptEncoder->tEncoder dispatchThreadgroups:tGridSize threadsPerThreadgroup:tThreadsPerGroup];
    }
}

static void
pl_bind_compute_bind_groups(
    plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst, uint32_t uCount,
    const plBindGroupHandle* atBindGroups, uint32_t uDynamicCount, const plDynamicBinding* ptDynamicBinding)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(uDynamicCount > 0)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
        [ptEncoder->tEncoder setBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:3];
    }

    // for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    // {
    //     [ptEncoder->tEncoder useHeap:ptDevice->sbtFrames[i].tDescriptorHeap];
    // }

    for(uint32_t i = 0; i < uCount; i++)
    {
        plMetalBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];
        
        for(uint32 j = 0; j < ptBindGroup->uHeapCount; j++)
        {
            [ptEncoder->tEncoder useHeap:ptBindGroup->atRequiredHeaps[j]];
        }

        [ptEncoder->tEncoder setBuffer:ptBindGroup->tShaderArgumentBuffer
            offset:ptBindGroup->uOffset
            atIndex:uFirst + i];
    }
}

static void
pl_bind_graphics_bind_groups(plRenderEncoder* ptEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, uint32_t uDynamicCount, const plDynamicBinding* ptDynamicBinding)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(uDynamicCount > 0)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
        [ptEncoder->tEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:3];
        [ptEncoder->tEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:3];
    }

    for(uint32_t i = 0; i < uCount; i++)
    {
        plMetalBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];

        for(uint32 j = 0; j < ptBindGroup->uHeapCount; j++)
        {
            [ptEncoder->tEncoder useHeap:ptBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
        }

        for(uint32_t k = 0; k < ptBindGroup->tLayout._uTextureBindingCount; k++)
        {
            const plTextureHandle tTextureHandle = ptBindGroup->atTextureBindings[k];
            plTexture* ptTexture = pl__get_texture(ptDevice, tTextureHandle);
            [ptEncoder->tEncoder useResource:ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];  
        }

        [ptEncoder->tEncoder setVertexBuffer:ptBindGroup->tShaderArgumentBuffer offset:ptBindGroup->uOffset atIndex:uFirst + i];
        [ptEncoder->tEncoder setFragmentBuffer:ptBindGroup->tShaderArgumentBuffer offset:ptBindGroup->uOffset atIndex:uFirst + i];
    }
}

static void
pl_set_depth_bias(plRenderEncoder* ptEncoder, float fDepthBiasConstantFactor, float fDepthBiasClamp, float fDepthBiasSlopeFactor)
{
    [ptEncoder->tEncoder setDepthBias:fDepthBiasConstantFactor slopeScale:fDepthBiasSlopeFactor clamp:fDepthBiasClamp];
}

static void
pl_set_viewport(plRenderEncoder* ptEncoder, const plRenderViewport* ptViewport)
{
    MTLViewport tViewport = {
        .originX = ptViewport->fX,
        .originY = ptViewport->fY,
        .width   = ptViewport->fWidth,
        .height  = ptViewport->fHeight,
        .znear   = ptViewport->fMinDepth,
        .zfar    = ptViewport->fMaxDepth
    };
    [ptEncoder->tEncoder setViewport:tViewport];
}

static void
pl_set_scissor_region(plRenderEncoder* ptEncoder, const plScissor* ptScissor)
{
    MTLScissorRect tScissorRect = {
        .x      = (NSUInteger)(ptScissor->iOffsetX),
        .y      = (NSUInteger)(ptScissor->iOffsetY),
        .width  = (NSUInteger)(ptScissor->uWidth),
        .height = (NSUInteger)(ptScissor->uHeight)
    };
    [ptEncoder->tEncoder setScissorRect:tScissorRect];
}

static void
pl_bind_vertex_buffer(plRenderEncoder* ptEncoder, plBufferHandle tHandle)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    [ptEncoder->tEncoder setVertexBuffer:ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer
        offset:0
        atIndex:4];
}

static void
pl_draw(plRenderEncoder* ptEncoder, uint32_t uCount, const plDraw* atDraws)
{

    for(uint32_t i = 0; i < uCount; i++)
    {
        [ptEncoder->tEncoder drawPrimitives:MTLPrimitiveTypeTriangle 
            vertexStart:atDraws[i].uVertexStart
            vertexCount:atDraws[i].uVertexCount
            instanceCount:atDraws[i].uInstanceCount
            baseInstance:atDraws[i].uInstance
            ];
    }
}

static void
pl_draw_indexed(plRenderEncoder* ptEncoder, uint32_t uCount, const plDrawIndex* atDraws)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    for(uint32_t i = 0; i < uCount; i++)
    {
        [ptEncoder->tEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
            indexCount:atDraws[i].uIndexCount
            indexType:MTLIndexTypeUInt32
            indexBuffer:ptDevice->sbtBuffersHot[atDraws[i].tIndexBuffer.uIndex].tBuffer
            indexBufferOffset:atDraws[i].uIndexStart * sizeof(uint32_t)
            instanceCount:atDraws[i].uInstanceCount
            baseVertex:atDraws[i].uVertexStart
            baseInstance:atDraws[i].uInstance
            ];
    }
}

static void
pl_bind_shader(plRenderEncoder* ptEncoder, plShaderHandle tHandle)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    [ptEncoder->tEncoder setStencilReferenceValue:ptMetalShader->ulStencilRef];
    [ptEncoder->tEncoder setCullMode:ptMetalShader->tCullMode];
    [ptEncoder->tEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [ptEncoder->tEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
    [ptEncoder->tEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
    [ptEncoder->tEncoder setTriangleFillMode:ptMetalShader->tFillMode];
}

static void
pl_bind_compute_shader(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle)
{
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plMetalComputeShader* ptMetalShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    [ptEncoder->tEncoder setComputePipelineState:ptMetalShader->tPipelineState];
}

static void
pl_draw_stream(plRenderEncoder* ptEncoder, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plCommandBuffer* ptCmdBuffer = ptEncoder->ptCommandBuffer;
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    // for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    // {
    //     [ptEncoder->tEncoder useHeap:ptDevice->sbtFrames[i].tDescriptorHeap stages:MTLRenderStageVertex | MTLRenderStageFragment];
    // }

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
        [ptEncoder->tEncoder setScissorRect:tScissorRect];

        MTLViewport tViewport = {
            .originX = ptArea->tViewport.fX,
            .originY = ptArea->tViewport.fY,
            .width   = ptArea->tViewport.fWidth,
            .height  = ptArea->tViewport.fHeight,
            .znear   = 0,
            .zfar    = 1.0
        };
        [ptEncoder->tEncoder setViewport:tViewport];

        const uint32_t uTokens = ptStream->_uStreamCount;
        uint32_t uCurrentStreamIndex = 0;
        uint32_t uTriangleCount = 0;
        plBufferHandle tIndexBuffer = {0};
        uint32_t uIndexBufferOffset = 0;
        uint32_t uVertexBufferOffset = 0;
        uint32_t uDynamicBufferOffset0 = 0;
        uint32_t uInstanceStart = 0;
        uint32_t uInstanceCount = 1;
        id<MTLDepthStencilState> tCurrentDepthStencilState = nil;

        uint32_t uDynamicSlot = UINT32_MAX;
        while(uCurrentStreamIndex < uTokens)
        {
            const uint32_t uDirtyMask = ptStream->_auStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                const plShaderHandle tShaderHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                const plShader* ptShader= &ptDevice->sbtShadersCold[tShaderHandle.uIndex];
                plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[tShaderHandle.uIndex];
                [ptEncoder->tEncoder setCullMode:ptMetalShader->tCullMode];
                [ptEncoder->tEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
                if(tCurrentDepthStencilState != ptMetalShader->tDepthStencilState)
                {
                    [ptEncoder->tEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
                }
                tCurrentDepthStencilState = ptMetalShader->tDepthStencilState;
                [ptEncoder->tEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
                [ptEncoder->tEncoder setTriangleFillMode:ptMetalShader->tFillMode];
                [ptEncoder->tEncoder setStencilReferenceValue:ptMetalShader->ulStencilRef];                

                uCurrentStreamIndex++;
                uDynamicSlot = ptShader->tDesc._uBindGroupLayoutCount;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET_0)
            {
                uDynamicBufferOffset0 = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];

                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [ptEncoder->tEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout._uTextureBindingCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptMetalBindGroup->atTextureBindings[k];
                    plTexture* ptTexture = pl__get_texture(ptDevice, tTextureHandle);
                    [ptEncoder->tEncoder useResource:ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];  
                }

                [ptEncoder->tEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:0];
                [ptEncoder->tEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:0];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];

                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [ptEncoder->tEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout._uTextureBindingCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptMetalBindGroup->atTextureBindings[k];
                    plTexture* ptTexture = pl__get_texture(ptDevice, tTextureHandle);
                    [ptEncoder->tEncoder useResource:ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];  
                }

                [ptEncoder->tEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:1];
                [ptEncoder->tEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:1];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];
                
                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [ptEncoder->tEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout._uTextureBindingCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptMetalBindGroup->atTextureBindings[k];
                    [ptEncoder->tEncoder useResource:ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment]; 
                }

                [ptEncoder->tEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:2];
                [ptEncoder->tEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:2];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER_0)
            {
                
                [ptEncoder->tEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptStream->_auStream[uCurrentStreamIndex]].tBuffer offset:0 atIndex:3];
                [ptEncoder->tEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptStream->_auStream[uCurrentStreamIndex]].tBuffer offset:0 atIndex:3];

                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET_0)
            {
                [ptEncoder->tEncoder setVertexBufferOffset:uDynamicBufferOffset0 atIndex:3];
                [ptEncoder->tEncoder setFragmentBufferOffset:uDynamicBufferOffset0 atIndex:3];
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
            {
                uIndexBufferOffset = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
            {
                uVertexBufferOffset = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
            {
                tIndexBuffer = (plBufferHandle){.uData = ptStream->_auStream[uCurrentStreamIndex] };
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER_0)
            {
                const plBufferHandle tBufferHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                [ptEncoder->tEncoder setVertexBuffer:ptDevice->sbtBuffersHot[tBufferHandle.uIndex].tBuffer
                    offset:0
                    atIndex:4];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
            {
                uTriangleCount = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_OFFSET)
            {
                uInstanceStart = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
            {
                uInstanceCount = ptStream->_auStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(tIndexBuffer.uData == 0)
            {
                [ptEncoder->tEncoder drawPrimitives:MTLPrimitiveTypeTriangle 
                    vertexStart:uVertexBufferOffset
                    vertexCount:uTriangleCount * 3
                    instanceCount:uInstanceCount
                    baseInstance:uInstanceStart
                    ];
            }
            else
            {
                [ptEncoder->tEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
                    indexCount:uTriangleCount * 3
                    indexType:MTLIndexTypeUInt32
                    indexBuffer:ptDevice->sbtBuffersHot[tIndexBuffer.uIndex].tBuffer
                    indexBufferOffset:uIndexBufferOffset * sizeof(uint32_t)
                    instanceCount:uInstanceCount
                    baseVertex:uVertexBufferOffset
                    baseInstance:uInstanceStart
                    ];
            }
        }
    }
    pl_end_profile_sample(0);
}

static void
pl_flush_device(plDevice* ptDevice)
{
    gptThreads->sleep_thread(500);
}

static void
pl_cleanup_graphics(void)
{
    pl__cleanup_common_graphics();
}

static void
pl_cleanup_surface(plSurface* ptSurface)
{
    PL_FREE(ptSurface);
}

static void
pl_cleanup_swapchain(plSwapchain* ptSwap)
{
    pl__cleanup_common_swapchain(ptSwap);
}

static void
pl_cleanup_device(plDevice* ptDevice)
{
    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtRenderPassesHot); i++)
    {
        for(uint32_t j = 0; j < gptGraphics->uFramesInFlight; j++)
        {
            pl_sb_free(ptDevice->sbtRenderPassesHot[i].atRenderPassDescriptors[j].sbptRenderPassDescriptor);
        }
    }
    pl_sb_free(ptDevice->sbtTexturesHot);
    pl_sb_free(ptDevice->sbtSamplersHot);
    pl_sb_free(ptDevice->sbtBindGroupsHot);
    pl_sb_free(ptDevice->sbtBuffersHot);
    pl_sb_free(ptDevice->sbtShadersHot);
    pl_sb_free(ptDevice->sbtRenderPassesHot);
    pl_sb_free(ptDevice->sbtRenderPassLayoutsHot);
    pl_sb_free(ptDevice->sbtComputeShadersHot);

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtFrames); i++)
    {
        plFrameContext* ptFrame = &ptDevice->sbtFrames[i];
        pl_sb_free(ptFrame->sbtDynamicBuffers);
    }
    pl_sb_free(ptDevice->sbtFrames);

    pl__cleanup_common_device(ptDevice);
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

static bool
pl__is_depth_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_D32_FLOAT:
        case PL_FORMAT_D32_FLOAT_S8_UINT:
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D16_UNORM_S8_UINT: return true;
    }
    return false;
}

static bool
pl__is_stencil_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_D32_FLOAT_S8_UINT:
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D16_UNORM_S8_UINT: return true;
    }
    return false;
}

static MTLBlendFactor
pl__metal_blend_factor(plBlendFactor tFactor)
{
    switch (tFactor)
    {
        case PL_BLEND_FACTOR_ZERO:                      return MTLBlendFactorZero;
        case PL_BLEND_FACTOR_ONE:                       return MTLBlendFactorOne;
        case PL_BLEND_FACTOR_SRC_COLOR:                 return MTLBlendFactorSourceColor;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:       return MTLBlendFactorOneMinusSourceColor;
        case PL_BLEND_FACTOR_DST_COLOR:                 return MTLBlendFactorDestinationColor;
        case PL_BLEND_FACTOR_ONE_MINUS_DST_COLOR:       return MTLBlendFactorOneMinusDestinationColor;
        case PL_BLEND_FACTOR_SRC_ALPHA:                 return MTLBlendFactorSourceAlpha;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:       return MTLBlendFactorOneMinusSourceAlpha;
        case PL_BLEND_FACTOR_DST_ALPHA:                 return MTLBlendFactorDestinationAlpha;
        case PL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:       return MTLBlendFactorOneMinusDestinationAlpha;
        case PL_BLEND_FACTOR_CONSTANT_COLOR:            return MTLBlendFactorBlendColor;
        case PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:  return MTLBlendFactorOneMinusBlendColor;
        case PL_BLEND_FACTOR_CONSTANT_ALPHA:            return MTLBlendFactorBlendAlpha;
        case PL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:  return MTLBlendFactorOneMinusBlendAlpha;
        case PL_BLEND_FACTOR_SRC_ALPHA_SATURATE:        return MTLBlendFactorSourceAlphaSaturated;
        case PL_BLEND_FACTOR_SRC1_COLOR:                return MTLBlendFactorSource1Color;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:      return MTLBlendFactorOneMinusSource1Color;
        case PL_BLEND_FACTOR_SRC1_ALPHA:                return MTLBlendFactorSource1Alpha;
        case PL_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:      return MTLBlendFactorOneMinusSource1Alpha;
    }
    PL_ASSERT(false && "Unsupported blend factor");
    return MTLBlendFactorZero;
}

static MTLBlendOperation
pl__metal_blend_op(plBlendOp tOp)
{
    switch (tOp)
    {
        case PL_BLEND_OP_ADD:              return MTLBlendOperationAdd;
        case PL_BLEND_OP_SUBTRACT:         return MTLBlendOperationSubtract;
        case PL_BLEND_OP_REVERSE_SUBTRACT: return MTLBlendOperationReverseSubtract;
        case PL_BLEND_OP_MIN:              return MTLBlendOperationMin;
        case PL_BLEND_OP_MAX:              return MTLBlendOperationMax;
    }
    PL_ASSERT(false && "Unsupported blend op");
    return MTLBlendOperationAdd;
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
        case PL_STORE_OP_STORE_MULTISAMPLE_RESOLVE: return MTLStoreActionMultisampleResolve;
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
pl__metal_wrap(plAddressMode tWrap)
{
    switch(tWrap)
    {
        case PL_ADDRESS_MODE_UNSPECIFIED:
        case PL_ADDRESS_MODE_WRAP:   return MTLSamplerAddressModeRepeat;
        case PL_ADDRESS_MODE_CLAMP:  return MTLSamplerAddressModeClampToEdge;
        case PL_ADDRESS_MODE_MIRROR: return MTLSamplerAddressModeMirrorRepeat;
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

static MTLStencilOperation
pl__metal_stencil_op(plStencilOp tOp)
{
    switch (tOp)
    {
        case PL_STENCIL_OP_KEEP:                return MTLStencilOperationKeep;
        case PL_STENCIL_OP_ZERO:                return MTLStencilOperationZero;
        case PL_STENCIL_OP_REPLACE:             return MTLStencilOperationReplace;
        case PL_STENCIL_OP_INCREMENT_AND_CLAMP: return MTLStencilOperationIncrementClamp;
        case PL_STENCIL_OP_DECREMENT_AND_CLAMP: return MTLStencilOperationDecrementClamp;
        case PL_STENCIL_OP_INVERT:              return MTLStencilOperationInvert;
        case PL_STENCIL_OP_INCREMENT_AND_WRAP:  return MTLStencilOperationIncrementWrap;
        case PL_STENCIL_OP_DECREMENT_AND_WRAP:  return MTLStencilOperationDecrementWrap;
    }
    PL_ASSERT(false && "Unsupported stencil op");
    return MTLStencilOperationKeep;
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
        case PL_FORMAT_R8G8_UNORM:         return MTLPixelFormatRG8Unorm;
        case PL_FORMAT_R8_UNORM:           return MTLPixelFormatR8Unorm;
        case PL_FORMAT_D32_FLOAT:          return MTLPixelFormatDepth32Float;
        case PL_FORMAT_D32_FLOAT_S8_UINT:  return MTLPixelFormatDepth32Float_Stencil8;
        case PL_FORMAT_D24_UNORM_S8_UINT:  return MTLPixelFormatDepth24Unorm_Stencil8;
    }

    PL_ASSERT(false && "Unsupported format");
    return MTLPixelFormatInvalid;
}

static MTLVertexFormat
pl__metal_vertex_format(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_R32G32B32A32_FLOAT: return MTLVertexFormatFloat4;
        case PL_FORMAT_R32G32B32_FLOAT:    return MTLVertexFormatFloat3;
        case PL_FORMAT_R32G32_FLOAT:       return MTLVertexFormatFloat2;

        case PL_FORMAT_B8G8R8A8_UNORM:
        case PL_FORMAT_R8G8B8A8_UNORM:     return MTLVertexFormatUChar4;

        case PL_FORMAT_R32_UINT:           return MTLVertexFormatUInt;
    }

    PL_ASSERT(false && "Unsupported vertex format");
    return MTLVertexFormatInvalid;
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

static void
pl__garbage_collect(plDevice* ptDevice)
{
    pl_begin_profile_sample(0, __FUNCTION__);
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPasses); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtRenderPasses[i].uIndex;
        plMetalRenderPass* ptMetalResource = &ptDevice->sbtRenderPassesHot[iResourceIndex];
        for(uint32_t uFrameIndex = 0; uFrameIndex < gptGraphics->uFramesInFlight; uFrameIndex++)
        {
            for(uint32_t j = 0; j < pl_sb_size(ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor); j++)
            {
                [ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[j] release];
                ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[j] = nil;
            }
            pl_sb_free(ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor);
        }
        pl_sb_push(ptDevice->sbtRenderPassFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPassLayouts); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtRenderPassLayouts[i].uIndex;
        plMetalRenderPassLayout* ptMetalResource = &ptDevice->sbtRenderPassLayoutsHot[iResourceIndex];
        pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtShaders); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtShaders[i].uIndex;
        plShader* ptResource = pl__get_shader(ptDevice, ptGarbage->sbtShaders[i]);

        plMetalShader* ptVariantMetalResource = &ptDevice->sbtShadersHot[ptGarbage->sbtShaders[i].uIndex];
        [ptVariantMetalResource->tDepthStencilState release];
        [ptVariantMetalResource->tRenderPipelineState release];
        ptVariantMetalResource->tDepthStencilState = nil;
        ptVariantMetalResource->tRenderPipelineState = nil;
        if(ptVariantMetalResource->tVertexLibrary)
        {
            [ptVariantMetalResource->tVertexLibrary release];
            ptVariantMetalResource->tVertexLibrary = nil;
        }
        if(ptVariantMetalResource->tFragmentLibrary)
        {
            [ptVariantMetalResource->tFragmentLibrary release];
            ptVariantMetalResource->tFragmentLibrary = nil;
        }
        pl_sb_push(ptDevice->sbtShaderFreeIndices, ptGarbage->sbtShaders[i].uIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
        plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[iResourceIndex];

        plMetalComputeShader* ptVariantMetalResource = &ptDevice->sbtComputeShadersHot[iResourceIndex];
        [ptVariantMetalResource->tPipelineState release];
        ptVariantMetalResource->tPipelineState = nil;
        if(ptVariantMetalResource->library)
        {
            [ptVariantMetalResource->library release];
            ptVariantMetalResource->library = nil;
        }
        pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, iResourceIndex);

    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBindGroups); i++)
    {
        const uint16_t iBindGroupIndex = ptGarbage->sbtBindGroups[i].uIndex;
        plMetalBindGroup* ptMetalResource = &ptDevice->sbtBindGroupsHot[iBindGroupIndex];
        [ptMetalResource->tShaderArgumentBuffer release];
        ptMetalResource->tShaderArgumentBuffer = nil;
        pl_sb_push(ptDevice->sbtBindGroupFreeIndices, iBindGroupIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtSamplers); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtSamplers[i].uIndex;
        plMetalSampler* ptMetalSampler = &ptDevice->sbtSamplersHot[iResourceIndex];
        [ptMetalSampler->tSampler release];
        ptMetalSampler->tSampler = nil;
        pl_sb_push(ptDevice->sbtSamplerFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint16_t uTextureIndex = ptGarbage->sbtTextures[i].uIndex;
        plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[uTextureIndex];
        [ptMetalTexture->tTexture release];
        ptMetalTexture->tTexture = nil;
        pl_sb_push(ptDevice->sbtTextureFreeIndices, uTextureIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBuffers); i++)
    {
        const uint16_t iBufferIndex = ptGarbage->sbtBuffers[i].uIndex;
        [ptDevice->sbtBuffersHot[iBufferIndex].tBuffer release];
        ptDevice->sbtBuffersHot[iBufferIndex].tBuffer = nil;
        pl_sb_push(ptDevice->sbtBufferFreeIndices, iBufferIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtMemory); i++)
    {
        if(ptGarbage->sbtMemory[i].ptAllocator)
        {
            ptGarbage->sbtMemory[i].ptAllocator->free(ptGarbage->sbtMemory[i].ptAllocator->ptInst, &ptGarbage->sbtMemory[i]);
        }
        else
        {
            pl_free_memory(ptDevice, &ptGarbage->sbtMemory[i]);
        }
    }

    pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtShaders);
    pl_sb_reset(ptGarbage->sbtComputeShaders);
    pl_sb_reset(ptGarbage->sbtRenderPasses);
    pl_sb_reset(ptGarbage->sbtRenderPassLayouts);
    pl_sb_reset(ptGarbage->sbtMemory);
    pl_sb_reset(ptGarbage->sbtBuffers);
    pl_sb_reset(ptGarbage->sbtBindGroups);
    pl_end_profile_sample(0);
}

//-----------------------------------------------------------------------------
// [SECTION] device memory allocators
//-----------------------------------------------------------------------------

static plDeviceMemoryAllocation
pl_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName)
{
    if(pcName == NULL)
    {
        pcName = "unnamed memory block";
    }

    plDeviceMemoryAllocation tBlock = {
        .uHandle = 0,
        .ulSize  = (uint64_t)szSize,
        .tMemoryMode = tMemoryMode
    };

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.size = tBlock.ulSize;
    ptHeapDescriptor.type = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;

    if(tMemoryMode == PL_MEMORY_GPU_CPU || tMemoryMode == PL_MEMORY_CPU)
    {
        ptHeapDescriptor.storageMode = MTLStorageModeShared;
        gptGraphics->szHostMemoryInUse += tBlock.ulSize;
    }
    else if(tMemoryMode == PL_MEMORY_GPU)
    {
        ptHeapDescriptor.storageMode = MTLStorageModePrivate;
        gptGraphics->szLocalMemoryInUse += tBlock.ulSize;
    }

    id<MTLHeap> tNewHeap = [ptDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    tNewHeap.label = [NSString stringWithUTF8String:pcName];
    tBlock.uHandle = (uint64_t)tNewHeap;
    
    [ptHeapDescriptor release];
    return tBlock;
}

static void
pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    id<MTLHeap> tHeap = (id<MTLHeap>)ptBlock->uHandle;

    [tHeap setPurgeableState:MTLPurgeableStateEmpty];
    [tHeap release];
    tHeap = nil;

    if(ptBlock->tMemoryMode == PL_MEMORY_GPU)
    {
        gptGraphics->szLocalMemoryInUse -= ptBlock->ulSize;
    }
    else
    {
        gptGraphics->szHostMemoryInUse -= ptBlock->ulSize;
    }
    ptBlock->uHandle = 0;
    ptBlock->pHostMapped = NULL;
    ptBlock->ulSize = 0;
    ptBlock->tMemoryMode = 0;
    ptBlock->ulMemoryType = 0;
}

static void
pl_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{

    ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBufferFreeIndices, tHandle.uIndex);

    [ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer release];
    ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer = nil;

    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    if(ptBuffer->tMemoryAllocation.ptAllocator)
        ptBuffer->tMemoryAllocation.ptAllocator->free(ptBuffer->tMemoryAllocation.ptAllocator->ptInst, &ptBuffer->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptBuffer->tMemoryAllocation);
}

static void
pl_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    pl_sb_push(ptDevice->sbtTextureFreeIndices, tHandle.uIndex);
    ptDevice->sbtTexturesCold[tHandle.uIndex]._uGeneration++;

    plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];
    [ptMetalTexture->tTexture release];
    ptMetalTexture->tTexture = nil;

    plTexture* ptTexture = &ptDevice->sbtTexturesCold[tHandle.uIndex];
    if(ptTexture->tMemoryAllocation.ptAllocator)
        ptTexture->tMemoryAllocation.ptAllocator->free(ptTexture->tMemoryAllocation.ptAllocator->ptInst, &ptTexture->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptTexture->tMemoryAllocation);
}

static void
pl_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    ptDevice->sbtBindGroupsCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBindGroupFreeIndices, tHandle.uIndex);

    plMetalBindGroup* ptMetalResource = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    [ptMetalResource->tShaderArgumentBuffer release];
    ptMetalResource->tShaderArgumentBuffer = nil;
}

static void
pl_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    ptDevice->sbtRenderPassesCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtRenderPassFreeIndices, tHandle.uIndex);

    plMetalRenderPass* ptMetalResource = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGraphics->uFramesInFlight; uFrameIndex++)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor); i++)
        {
            [ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[i] release];
            ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[i] = nil;
        }
        pl_sb_free(ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor);
    }
}

static void
pl_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    ptDevice->sbtShadersCold[tHandle.uIndex]._uGeneration++;

    plShader* ptResource = &ptDevice->sbtShadersCold[tHandle.uIndex];

    plMetalShader* ptVariantMetalResource = &ptDevice->sbtShadersHot[tHandle.uIndex];
    [ptVariantMetalResource->tDepthStencilState release];
    [ptVariantMetalResource->tRenderPipelineState release];
    ptVariantMetalResource->tDepthStencilState = nil;
    ptVariantMetalResource->tRenderPipelineState = nil;
    pl_sb_push(ptDevice->sbtShaderFreeIndices, tHandle.uIndex);

}

static void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    ptDevice->sbtComputeShadersCold[tHandle.uIndex]._uGeneration++;

    plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[tHandle.uIndex];
    plMetalComputeShader* ptVariantMetalResource = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    [ptVariantMetalResource->tPipelineState release];
    ptVariantMetalResource->tPipelineState = nil;
    pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, tHandle.uIndex);
}
