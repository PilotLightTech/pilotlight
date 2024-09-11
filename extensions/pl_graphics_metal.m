/*
   pl_metal_ext.m
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
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

#include "pilot_light.h"
#include "pl_os.h"
#include "pl_profile.h"
#include "pl_memory.h"
#include "pl_ext.inc"
#include "pl_graphics_internal.h"

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
// [SECTION] internal structs & types
//-----------------------------------------------------------------------------

typedef struct _plCommandBuffer
{
    plBeginCommandInfo   tBeginInfo;
    plDevice*            ptDevice;
    id<MTLCommandBuffer> tCmdBuffer;
} plCommandBuffer;

typedef struct _plRenderEncoder
{
    plCommandBufferHandle       tCommandBuffer;
    plRenderPassHandle          tRenderPassHandle;
    uint32_t                    _uCurrentSubpass;
    id<MTLRenderCommandEncoder> tEncoder;
} plRenderEncoder;

typedef struct _plComputeEncoder
{
    plCommandBufferHandle tCommandBuffer;
    id<MTLComputeCommandEncoder> tEncoder;
} plComputeEncoder;

typedef struct _plBlitEncoder
{
    plCommandBufferHandle tCommandBuffer;
    id<MTLBlitCommandEncoder> tEncoder;
} plBlitEncoder;

typedef struct _plMetalDynamicBuffer
{
    uint32_t                 uByteOffset;
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
} plMetalRenderPass;

typedef struct _plMetalBuffer
{
    id<MTLBuffer> tBuffer;
    id<MTLHeap>   tHeap;
} plMetalBuffer;

typedef struct _plFrameContext
{
    // temporary bind group stuff
    uint32_t       uCurrentArgumentBuffer;
    plMetalBuffer* sbtArgumentBuffers;
    size_t         szCurrentArgumentOffset;

    // dynamic buffer stuff
    uint32_t              uCurrentBufferIndex;
    plMetalDynamicBuffer* sbtDynamicBuffers;

    dispatch_semaphore_t tFrameBoundarySemaphore;

    id<MTLHeap> tDescriptorHeap;
    uint64_t    ulDescriptorHeapOffset;
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

typedef struct _plMetalTimelineSemaphore
{
    id<MTLEvent>       tEvent;
    id<MTLSharedEvent> tSharedEvent;
} plMetalTimelineSemaphore;

typedef struct _plMetalBindGroup
{
    id<MTLBuffer> tShaderArgumentBuffer;
    plBindGroupLayout tLayout;
    plTextureHandle atTextureBindings[PL_MAX_TEXTURES_PER_BIND_GROUP];
    plBufferHandle  aBufferBindings[PL_MAX_BUFFERS_PER_BIND_GROUP];
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
    
    // command buffers
    plCommandBuffer* sbtCommandBuffers;
    uint32_t*        sbuCommandBuffersFreeIndices;

    // render encoders
    plRenderEncoder* sbtRenderEncoders;
    uint32_t*        sbuRenderEncodersFreeIndices;

    // blit encoders
    plBlitEncoder* sbtBlitEncoders;
    uint32_t*      sbuBlitEncodersFreeIndices;

    // compute encoders
    plComputeEncoder* sbtComputeEncoders;
    uint32_t*         sbuComputeEncodersFreeIndices;

    // metal specifics
    plTempAllocator     tTempAllocator;
    id<MTLCommandQueue> tCmdQueue;
    CAMetalLayer*       pMetalLayer;
    id<MTLFence>        tFence;
    
    // per frame
    id<CAMetalDrawable> tCurrentDrawable;
} plGraphics;

typedef struct _plDevice
{
    plFrameGarbage*           sbtGarbage;
    plFrameContext*           sbFrames;
    bool                      bDescriptorIndexing;
    plDeviceMemoryAllocatorI* ptDynamicAllocator;
    void*                     _pInternalData;

    // render pass layouts
    plRenderPassLayoutHandle tMainRenderPassLayout;
    plMetalRenderPassLayout* sbtRenderPassLayoutsHot;
    plRenderPassLayout*      sbtRenderPassLayoutsCold;
    uint32_t*                sbtRenderPassLayoutGenerations;
    uint32_t*                sbtRenderPassLayoutFreeIndices;

    // render passes
    plRenderPassHandle tMainRenderPass;
    plMetalRenderPass* sbtRenderPassesHot;
    plRenderPass*      sbtRenderPassesCold;
    uint32_t*          sbtRenderPassGenerations;
    uint32_t*          sbtRenderPassFreeIndices;

    // shaders
    plMetalShader* sbtShadersHot;
    plShader*      sbtShadersCold;
    uint32_t*      sbtShaderGenerations;
    uint32_t*      sbtShaderFreeIndices;

    // compute shaders
    plMetalComputeShader* sbtComputeShadersHot;
    plComputeShader*      sbtComputeShadersCold;
    uint32_t*             sbtComputeShaderGenerations;
    uint32_t*             sbtComputeShaderFreeIndices;

    // buffers
    plMetalBuffer* sbtBuffersHot;
    plBuffer*      sbtBuffersCold;
    uint32_t*      sbtBufferGenerations;
    uint32_t*      sbtBufferFreeIndices;

    // textures
    plMetalTexture* sbtTexturesHot;
    plTexture*      sbtTexturesCold;
    uint32_t*       sbtTextureGenerations;
    uint32_t*       sbtTextureFreeIndices;

    // samplers
    plMetalSampler* sbtSamplersHot;
    plSampler*      sbtSamplersCold;
    uint32_t*       sbtSamplerGenerations;
    uint32_t*       sbtSamplerFreeIndices;

    // bind groups
    plMetalBindGroup*  sbtBindGroupsHot;
    plBindGroup*       sbtBindGroupsCold;
    uint32_t*          sbtBindGroupGenerations;
    uint32_t*          sbtBindGroupFreeIndices;
    plBindGroupHandle* sbtFreeDrawBindGroups;

    // timeline semaphores
    plMetalTimelineSemaphore* sbtSemaphoresHot;
    uint32_t*                 sbtSemaphoreGenerations;
    uint32_t*                 sbtSemaphoreFreeIndices;

    // metal specifics
    id<MTLDevice> tDevice;
    
} plDevice;

typedef struct _plSwapchain
{
    plDevice*        ptDevice;
    plExtent         tExtent;
    plFormat         tFormat;
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
static MTLSamplerAddressMode  pl__metal_wrap(plWrapMode tWrap);
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
static void                   pl__garbage_collect(plDevice* ptDevice);

static plDeviceMemoryAllocation pl_allocate_memory(plDevice* ptDevice, size_t ulSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName);
static void pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plRenderPassLayoutHandle
pl_create_render_pass_layout(plDevice* ptDevice, const plRenderPassLayoutDescription* ptDesc)
{

    plRenderPassLayoutHandle tHandle = pl__get_new_render_pass_layout_handle(ptDevice);

    plRenderPassLayout tLayout = {
        .tDesc = *ptDesc
    };

    ptDevice->sbtRenderPassLayoutsHot[tHandle.uIndex] = (plMetalRenderPassLayout){0};
    ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex] = tLayout;
    return tHandle;
}

static void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{

    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[tHandle.uIndex];
    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    ptRenderPass->tDesc.tDimensions = tDimensions;

    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];

    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGraphics->uFramesInFlight; uFrameIndex++)
    {
        for(uint32_t i = 0; i < ptLayout->tDesc.uSubpassCount; i++)
        {
            const plSubpass* ptSubpass = &ptLayout->tDesc.atSubpasses[i];
            MTLRenderPassDescriptor* ptRenderPassDescriptor = ptMetalRenderPass->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[i];

            uint32_t uCurrentColorAttachment = 0;
            for(uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
            {
                uint32_t uTargetIndex = ptSubpass->auRenderTargets[j];
                const uint32_t uTextureIndex = ptAttachments[uFrameIndex].atViewAttachments[uTargetIndex].uIndex;
                if(pl__is_depth_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
                {
                    ptRenderPassDescriptor.depthAttachment.texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    ptRenderPassDescriptor.depthAttachment.slice = ptDevice->sbtTexturesCold[uTextureIndex].tView.uBaseLayer;
                    if(pl__is_stencil_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
                    {
                        ptRenderPassDescriptor.stencilAttachment.texture = ptDevice->sbtTexturesHot[uTextureIndex].tTexture;
                    }
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
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDescription* ptDesc, const plRenderPassAttachments* ptAttachments)
{
    plRenderPassHandle tHandle = pl__get_new_render_pass_handle(ptDevice);

    plRenderPass tRenderPass = {
        .tDesc = *ptDesc
    };

    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptDesc->tLayout.uIndex];

    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[tHandle.uIndex];
    ptMetalRenderPass->tFence = [ptDevice->tDevice newFence];

    // subpasses
    for(uint32_t uFrameIndex = 0; uFrameIndex < gptGraphics->uFramesInFlight; uFrameIndex++)
    {
        bool abTargetSeen[PL_MAX_RENDER_TARGETS] = {0};
        for(uint32_t i = 0; i < ptLayout->tDesc.uSubpassCount; i++)
        {
            const plSubpass* ptSubpass = &ptLayout->tDesc.atSubpasses[i];

            MTLRenderPassDescriptor* ptRenderPassDescriptor = [MTLRenderPassDescriptor new];
            // uint32_t auLastFrames[PL_MAX_RENDER_TARGETS] = {0};
            
            uint32_t uCurrentColorAttachment = 0;
            for(uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
            {
                uint32_t uTargetIndex = ptSubpass->auRenderTargets[j];
                const uint32_t uTextureIndex = ptAttachments[uFrameIndex].atViewAttachments[uTargetIndex].uIndex;
                if(pl__is_depth_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
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


                    if(i == ptLayout->tDesc.uSubpassCount - 1)
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
                    ptLayout->tDesc.atSubpasses[i]._bHasDepth = true;
                }
                else
                {
                    const uint32_t uTargetIndexOriginal = uTargetIndex;
                    if(ptLayout->tDesc.atSubpasses[i]._bHasDepth)
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

                    if(i == ptLayout->tDesc.uSubpassCount - 1)
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

    ptDevice->sbtRenderPassesCold[tHandle.uIndex] = tRenderPass;
    return tHandle;
}

static void
pl_copy_buffer_to_texture(plBlitEncoderHandle tEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
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
        tSize.width  = ptRegions[i].tImageExtent.uWidth;
        tSize.height = ptRegions[i].tImageExtent.uHeight;
        tSize.depth  = ptRegions[i].tImageExtent.uDepth;

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
pl_copy_texture_to_buffer(plBlitEncoderHandle tEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
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
        tSize.width  = ptRegions[i].tImageExtent.uWidth;
        tSize.height = ptRegions[i].tImageExtent.uHeight;
        tSize.depth  = ptRegions[i].tImageExtent.uDepth;

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
pl_copy_buffer(plBlitEncoderHandle tEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint32_t uSourceOffset, uint32_t uDestinationOffset, size_t szSize)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    [ptEncoder->tEncoder copyFromBuffer:ptDevice->sbtBuffersHot[tSource.uIndex].tBuffer sourceOffset:uSourceOffset toBuffer:ptDevice->sbtBuffersHot[tDestination.uIndex].tBuffer destinationOffset:uDestinationOffset size:szSize];
}

static plSemaphoreHandle
pl_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plSemaphoreHandle tHandle = pl__get_new_semaphore_handle(ptDevice);
    
    plMetalTimelineSemaphore tSemaphore = {0};
    if(bHostVisible)
    {
        tSemaphore.tSharedEvent = [ptDevice->tDevice newSharedEvent];
    }
    else
    {
        tSemaphore.tEvent = [ptDevice->tDevice newEvent];
    }
    ptDevice->sbtSemaphoresHot[tHandle.uIndex] = tSemaphore;
    return tHandle;
}

static void
pl_signal_semaphore(plDevice* ptDevice, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    PL_ASSERT(ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent != nil);
    if(ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent)
    {
        ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent.signaledValue = ulValue;
    }
}

static void
pl_wait_semaphore(plDevice* ptDevice, plSemaphoreHandle tHandle, uint64_t ulValue)
{
    PL_ASSERT(ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent != nil);
    if(ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent)
    {
        while(ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent.signaledValue != ulValue)
        {
            gptThreads->sleep_thread(1);
        }
    }
}

static uint64_t
pl_get_semaphore_value(plDevice* ptDevice, plSemaphoreHandle tHandle)
{
    PL_ASSERT(ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent != nil);

    if(ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent)
    {
        return ptDevice->sbtSemaphoresHot[tHandle.uIndex].tSharedEvent.signaledValue;
    }
    return 0;
}

static plBufferHandle
pl_create_buffer(plDevice* ptDevice, const plBufferDescription* ptDesc, const char* pcName)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);

    plBuffer tBuffer = {
        .tDescription = *ptDesc
    };

    if(pcName)
    {
        pl_sprintf(tBuffer.tDescription.acDebugName, "%s", pcName);
    }

    MTLResourceOptions tStorageMode = MTLResourceStorageModePrivate;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_STAGING)
    {
        tStorageMode = MTLResourceStorageModeShared;
    }

    MTLSizeAndAlign tSizeAndAlign = [ptDevice->tDevice heapBufferSizeAndAlignWithLength:ptDesc->uByteSize options:tStorageMode];
    tBuffer.tMemoryRequirements.ulSize = tSizeAndAlign.size;
    tBuffer.tMemoryRequirements.ulAlignment = tSizeAndAlign.align;
    tBuffer.tMemoryRequirements.uMemoryTypeBits = 0;

    plMetalBuffer tMetalBuffer = {
        0
    };
    ptDevice->sbtBuffersHot[tHandle.uIndex] = tMetalBuffer;
    ptDevice->sbtBuffersCold[tHandle.uIndex] = tBuffer;
    return tHandle;
}

static void
pl_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];

    MTLResourceOptions tStorageMode = MTLResourceStorageModeShared;
    if(ptAllocation->tMemoryMode == PL_MEMORY_GPU)
    {
        tStorageMode = MTLResourceStorageModePrivate;
    }

    ptMetalBuffer->tBuffer = [(id<MTLHeap>)ptAllocation->uHandle newBufferWithLength:ptAllocation->ulSize options:tStorageMode offset:ptAllocation->ulOffset];
    ptMetalBuffer->tBuffer.label = [NSString stringWithUTF8String:ptBuffer->tDescription.acDebugName];

    if(ptAllocation->tMemoryMode != PL_MEMORY_GPU)
    {
        memset(ptMetalBuffer->tBuffer.contents, 0, ptAllocation->ulSize);
        ptBuffer->tMemoryAllocation.pHostMapped = ptMetalBuffer->tBuffer.contents;
    }
    ptMetalBuffer->tHeap = (id<MTLHeap>)ptAllocation->uHandle;
}

static void
pl_generate_mipmaps(plBlitEncoderHandle tEncoder, plTextureHandle tTexture)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptTexture = pl__get_texture(ptDevice, tTexture);
    if(ptTexture->tDesc.uMips < 2)
        return;

    [ptEncoder->tEncoder generateMipmapsForTexture:ptDevice->sbtTexturesHot[tTexture.uIndex].tTexture];
}

static plTextureHandle
pl_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, const char* pcName)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);

    plTextureDesc tDesc = *ptDesc;

    if(tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;

    plTexture tTexture = {
        .tDesc = tDesc,
        .tView = {
            .tFormat = tDesc.tFormat,
            .uBaseMip = 0,
            .uMips = tDesc.uMips,
            .uBaseLayer = 0,
            .uLayerCount = tDesc.uLayers,
            .tTexture = tHandle
        }
    };

    MTLTextureDescriptor* ptTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    ptTextureDescriptor.pixelFormat = pl__metal_format(tDesc.tFormat);
    ptTextureDescriptor.width = tDesc.tDimensions.x;
    ptTextureDescriptor.height = tDesc.tDimensions.y;
    ptTextureDescriptor.mipmapLevelCount = tDesc.uMips;
    ptTextureDescriptor.arrayLength = tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY ? tDesc.uLayers : 1;
    ptTextureDescriptor.depth = tDesc.tDimensions.z;
    ptTextureDescriptor.sampleCount = 1;

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

    MTLSizeAndAlign tSizeAndAlign = [ptDevice->tDevice heapTextureSizeAndAlignWithDescriptor:ptTextureDescriptor];
    tTexture.tMemoryRequirements.ulAlignment = tSizeAndAlign.align;
    tTexture.tMemoryRequirements.ulSize = tSizeAndAlign.size;
    tTexture.tMemoryRequirements.uMemoryTypeBits = 0;
    plMetalTexture tMetalTexture = {
        .ptTextureDescriptor = ptTextureDescriptor,
        .bOriginalView = true
    };
    ptDevice->sbtTexturesHot[tHandle.uIndex] = tMetalTexture;
    ptDevice->sbtTexturesCold[tHandle.uIndex] = tTexture;
    return tHandle;
}

static plSamplerHandle
pl_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc, const char* pcName)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);

    plSampler tSampler = {
        .tDesc = *ptDesc
    };

    MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
    samplerDesc.minFilter = pl__metal_filter(ptDesc->tFilter);
    samplerDesc.magFilter = pl__metal_filter(ptDesc->tFilter);
    samplerDesc.mipFilter = ptDesc->tMipmapMode == PL_MIPMAP_MODE_LINEAR ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
    samplerDesc.normalizedCoordinates = YES;
    samplerDesc.supportArgumentBuffers = YES;
    samplerDesc.sAddressMode = pl__metal_wrap(ptDesc->tHorizontalWrap);
    samplerDesc.tAddressMode = pl__metal_wrap(ptDesc->tVerticalWrap);
    samplerDesc.rAddressMode = samplerDesc.sAddressMode;
    samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack;
    samplerDesc.compareFunction = pl__metal_compare(ptDesc->tCompare);
    samplerDesc.lodMinClamp = ptDesc->fMinMip;
    samplerDesc.lodMaxClamp = ptDesc->fMaxMip;
    samplerDesc.label = [NSString stringWithUTF8String:pcName];
    samplerDesc.compareFunction = MTLCompareFunctionNever;
    samplerDesc.maxAnisotropy = ptDesc->fMaxAnisotropy;
    if(ptDesc->fMaxAnisotropy == 0.0f)
        samplerDesc.maxAnisotropy = 16.0f;

    plMetalSampler tMetalSampler = {
        .tSampler = [ptDevice->tDevice newSamplerStateWithDescriptor:samplerDesc]
    };

    ptDevice->sbtSamplersHot[tHandle.uIndex] = tMetalSampler;
    ptDevice->sbtSamplersCold[tHandle.uIndex] = tSampler;
    return tHandle;
}

static plBindGroupHandle
pl_get_temporary_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    if(pcName == NULL)
        pcName = "unnamed temporary bind group";

    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    uint32_t uDescriptorCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;

    for(uint32_t i = 0; i < ptLayout->uTextureBindingCount; i++)
    {
        uint32_t uCurrentDescriptorCount = ptLayout->atTextureBindings[i].uDescriptorCount;
        if(uCurrentDescriptorCount== 0)
            uCurrentDescriptorCount = 1;
        if(uCurrentDescriptorCount > 1)
            uDescriptorCount += ptLayout->atTextureBindings[i].uDescriptorCount - 1;
    }

    NSUInteger argumentBufferLength = sizeof(uint64_t) * uDescriptorCount;

    if(argumentBufferLength + ptFrame->szCurrentArgumentOffset > PL_DYNAMIC_ARGUMENT_BUFFER_SIZE)
    {
        ptFrame->uCurrentArgumentBuffer++;
        if(ptFrame->uCurrentArgumentBuffer >= pl_sb_size(ptFrame->sbtArgumentBuffers))
        {
            plMetalBuffer tArgumentBuffer = {
                .tBuffer = [ptFrame->tDescriptorHeap newBufferWithLength:PL_DYNAMIC_ARGUMENT_BUFFER_SIZE options:MTLResourceStorageModeShared offset:ptFrame->ulDescriptorHeapOffset]
            };
            ptFrame->ulDescriptorHeapOffset += PL_DYNAMIC_ARGUMENT_BUFFER_SIZE;
            ptFrame->ulDescriptorHeapOffset = PL__ALIGN_UP(ptFrame->ulDescriptorHeapOffset, 256);

            pl_sb_push(ptFrame->sbtArgumentBuffers, tArgumentBuffer);
        }
         ptFrame->szCurrentArgumentOffset = 0;
    }

    plMetalBindGroup tMetalBindGroup = {
        .tShaderArgumentBuffer = ptFrame->sbtArgumentBuffers[ptFrame->uCurrentArgumentBuffer].tBuffer,
        .uOffset = ptFrame->szCurrentArgumentOffset,
    };
    ptFrame->szCurrentArgumentOffset += argumentBufferLength;
    [tMetalBindGroup.tShaderArgumentBuffer retain];
    tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:pcName];

    ptDevice->sbtBindGroupsHot[tHandle.uIndex] = tMetalBindGroup;
    ptDevice->sbtBindGroupsCold[tHandle.uIndex] = tBindGroup;
    pl_queue_bind_group_for_deletion(ptDevice, tHandle);
    return tHandle;
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    if(pcName == NULL)
        pcName = "unnamed bind group";

    uint32_t uBindGroupIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtBindGroupFreeIndices) > 0)
        uBindGroupIndex = pl_sb_pop(ptDevice->sbtBindGroupFreeIndices);
    else
    {
        uBindGroupIndex = pl_sb_size(ptDevice->sbtBindGroupsCold);
        pl_sb_add(ptDevice->sbtBindGroupsCold);
        pl_sb_push(ptDevice->sbtBindGroupGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtBindGroupsHot);
    }

    plBindGroupHandle tHandle = {
        .uGeneration = ++ptDevice->sbtBindGroupGenerations[uBindGroupIndex],
        .uIndex = uBindGroupIndex
    };

    plBindGroup tBindGroup = {
        .tLayout = *ptLayout
    };

    uint32_t uDescriptorCount = ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount;

    for(uint32_t i = 0; i < ptLayout->uTextureBindingCount; i++)
    {
        uint32_t uCurrentDescriptorCount = ptLayout->atTextureBindings[i].uDescriptorCount;
        if(uCurrentDescriptorCount== 0)
            uCurrentDescriptorCount = 1;
        if(uCurrentDescriptorCount > 1)
            uDescriptorCount += ptLayout->atTextureBindings[i].uDescriptorCount - 1;
    }

    NSUInteger argumentBufferLength = sizeof(uint64_t) * uDescriptorCount;
    MTLSizeAndAlign tSizeAlign = [ptDevice->tDevice heapBufferSizeAndAlignWithLength:argumentBufferLength options:MTLResourceStorageModeShared];

    PL_ASSERT(ptFrame->ulDescriptorHeapOffset + tSizeAlign.size < PL_DEVICE_ALLOCATION_BLOCK_SIZE);

    plMetalBindGroup tMetalBindGroup = {
        .tShaderArgumentBuffer = [ptFrame->tDescriptorHeap newBufferWithLength:tSizeAlign.size options:MTLResourceStorageModeShared offset:ptFrame->ulDescriptorHeapOffset]
    };
    tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:pcName];
    [tMetalBindGroup.tShaderArgumentBuffer retain];
    ptFrame->ulDescriptorHeapOffset += tSizeAlign.size;
    ptFrame->ulDescriptorHeapOffset = PL__ALIGN_UP(ptFrame->ulDescriptorHeapOffset, tSizeAlign.align);

    ptDevice->sbtBindGroupsHot[uBindGroupIndex] = tMetalBindGroup;
    ptDevice->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    return tHandle;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{

    plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    plBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsCold[tHandle.uIndex];


    ptMetalBindGroup->uHeapCount = 0;
    const char* pcDescriptorStart = ptMetalBindGroup->tShaderArgumentBuffer.contents;

    uint64_t* pulDescriptorStart = (uint64_t*)&pcDescriptorStart[ptMetalBindGroup->uOffset];

    for(uint32_t i = 0; i < ptData->uBufferCount; i++)
    {
        const plBindGroupUpdateBufferData* ptUpdate = &ptData->atBuffers[i];
        plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[ptUpdate->tBuffer.uIndex];
        uint64_t* ppfDestination = &pulDescriptorStart[ptUpdate->uSlot];
        *ppfDestination = ptMetalBuffer->tBuffer.gpuAddress;
        ptMetalBindGroup->aBufferBindings[i] = ptUpdate->tBuffer;

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
        const plBindGroupUpdateTextureData* ptUpdate = &ptData->atTextures[i];
        plTexture* ptTexture = &ptDevice->sbtTexturesCold[ptUpdate->tTexture.uIndex];
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

static void
pl_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plTexture* ptTexture = &ptDevice->sbtTexturesCold[tHandle.uIndex];
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
    ptMetalTexture->tTexture.label = [NSString stringWithUTF8String:ptTexture->tDesc.acDebugName];
    [ptMetalTexture->ptTextureDescriptor release];
    ptMetalTexture->ptTextureDescriptor = nil;

    if(ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
    {
        if(pl_sb_size(ptDevice->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptDevice->sbtFreeDrawBindGroups);
        }

        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = tHandle,
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 1,
            .atTextures = atBGTextureData
        };
        pl_update_bind_group(ptDevice, ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }
}

static plTextureHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const char* pcName)
{
    uint32_t uTextureIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtTextureFreeIndices) > 0)
        uTextureIndex = pl_sb_pop(ptDevice->sbtTextureFreeIndices);
    else
    {
        uTextureIndex = pl_sb_size(ptDevice->sbtTexturesCold);
        pl_sb_add(ptDevice->sbtTexturesCold);
        pl_sb_push(ptDevice->sbtTextureGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptDevice->sbtTextureGenerations[uTextureIndex],
        .uIndex = uTextureIndex
    };

    plTexture tTexture = {
        .tDesc = ptDevice->sbtTexturesCold[ptViewDesc->tTexture.uIndex].tDesc,
        .tView = *ptViewDesc
    };
    tTexture.tDesc.uMips = ptViewDesc->uMips;
    tTexture.tDesc.uLayers = ptViewDesc->uLayerCount;
    tTexture.tView.uBaseMip = 0;
    tTexture.tView.uBaseLayer = 0;

    plTexture* ptTexture = pl__get_texture(ptDevice, ptViewDesc->tTexture);
    plMetalTexture* ptOldMetalTexture = &ptDevice->sbtTexturesHot[ptViewDesc->tTexture.uIndex];

    if(ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
    {
        if(pl_sb_size(ptDevice->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptDevice->sbtFreeDrawBindGroups);
        }

        const plBindGroupUpdateTextureData atBGTextureData[] = {
            {
                .tTexture = tHandle,
                .uSlot    = 0,
                .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
            }
        };
        const plBindGroupUpdateData tBGData = {
            .uTextureCount = 1,
            .atTextures = atBGTextureData
        };
        pl_update_bind_group(ptDevice, ptDevice->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }

    plMetalTexture* ptNewMetalTexture = &ptDevice->sbtTexturesHot[uTextureIndex];
    ptNewMetalTexture->bOriginalView = false;

    MTLTextureType tTextureType = MTLTextureType2D;

    if(tTexture.tDesc.tType == PL_TEXTURE_TYPE_2D)
        tTextureType = MTLTextureType2D;
    else if(tTexture.tDesc.tType == PL_TEXTURE_TYPE_CUBE)
        tTextureType = MTLTextureTypeCube;
    else if(tTexture.tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
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
        .length = ptViewDesc->uMips == 0 ? ptTexture->tDesc.uMips - ptViewDesc->uBaseMip : ptViewDesc->uMips,
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


    if(pcName == NULL)
        pcName = "unnamed texture";

    ptNewMetalTexture->tTexture = [ptOldMetalTexture->tTexture newTextureViewWithPixelFormat:pl__metal_format(ptViewDesc->tFormat) 
            textureType:tTextureType
            levels:tLevelRange
            slices:tSliceRange];

    ptNewMetalTexture->tTexture.label = [NSString stringWithUTF8String:pcName];

    // ptNewMetalTexture->tTexture = ptOldMetalTexture->tTexture;
    ptNewMetalTexture->tHeap = ptOldMetalTexture->tHeap;

    ptDevice->sbtTexturesCold[uTextureIndex] = tTexture;
    return tHandle;
}

static plDynamicBinding
pl_allocate_dynamic_data(plDevice* ptDevice, size_t szSize)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

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
            pl_sprintf(atNameBuffer, "D-BUF-F%d-%d", (int)gptGraphics->uCurrentFrameIndex, (int)ptFrame->uCurrentBufferIndex);

            ptDynamicBuffer->tMemory = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, 0, PL_DEVICE_ALLOCATION_BLOCK_SIZE, 0, atNameBuffer);
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
pl_create_compute_shader(plDevice* ptDevice, const plComputeShaderDescription* ptDescription)
{
    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);

    plComputeShader tShader = {
        .tDescription = *ptDescription
    };

    plMetalComputeShader* ptMetalShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];

    if(tShader.tDescription.tShader.pcEntryFunc == NULL)
        tShader.tDescription.tShader.pcEntryFunc = "kernel_main";

    NSString* entryFunc = [NSString stringWithUTF8String:"kernel_main"];

    // compile shader source
    NSError* error = nil;
    NSString* shaderSource = [NSString stringWithUTF8String:(const char*)tShader.tDescription.tShader.puCode];
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];
    ptMetalShader->library = [ptDevice->tDevice  newLibraryWithSource:shaderSource options:ptCompileOptions error:&error];
    if (ptMetalShader->library == nil)
    {
        NSLog(@"Error: failed to create Metal library: %@", error);
    }
    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
    }

    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptDescription->pTempConstantData;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
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
    ptDevice->sbtComputeShadersCold[tHandle.uIndex] = tShader;
    return tHandle;
}

static plShaderHandle
pl_create_shader(plDevice* ptDevice, const plShaderDescription* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);

    plShader tShader = {
        .tDescription = *ptDescription
    };

    plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    if(tShader.tDescription.tPixelShader.pcEntryFunc == NULL)
        tShader.tDescription.tPixelShader.pcEntryFunc = "fragment_main";

    if(tShader.tDescription.tVertexShader.pcEntryFunc == NULL)
        tShader.tDescription.tVertexShader.pcEntryFunc = "vertex_main";

    NSString* vertexEntry = [NSString stringWithUTF8String:"vertex_main"];
    NSString* fragmentEntry = [NSString stringWithUTF8String:"fragment_main"];

    // vertex layout
    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.layouts[4].stepRate = 1;
    vertexDescriptor.layouts[4].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[4].stride = ptDescription->tVertexBufferBinding.uByteStride;

    uint32_t uCurrentAttributeCount = 0;
    for(uint32_t i = 0; i < PL_MAX_VERTEX_ATTRIBUTES; i++)
    {
        if(ptDescription->tVertexBufferBinding.atAttributes[i].tFormat == PL_FORMAT_UNKNOWN)
            break;
        vertexDescriptor.attributes[i].bufferIndex = 4;
        vertexDescriptor.attributes[i].offset = ptDescription->tVertexBufferBinding.atAttributes[i].uByteOffset;
        vertexDescriptor.attributes[i].format = pl__metal_vertex_format(ptDescription->tVertexBufferBinding.atAttributes[i].tFormat);
        uCurrentAttributeCount++;
    }

    // prepare preprocessor defines
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];
    ptCompileOptions.fastMathEnabled = false;

    // compile shader source
    NSError* error = nil;
    NSString* vertexSource = [NSString stringWithUTF8String:(const char*)tShader.tDescription.tVertexShader.puCode];
    ptMetalShader->tVertexLibrary = [ptDevice->tDevice  newLibraryWithSource:vertexSource options:ptCompileOptions error:&error];
    if (ptMetalShader->tVertexLibrary == nil)
    {
        NSLog(@"Error: failed to create Metal vertex library: %@", error);
    }

    if(tShader.tDescription.tPixelShader.puCode)
    {
        NSString* fragmentSource = [NSString stringWithUTF8String:(const char*)tShader.tDescription.tPixelShader.puCode];
        ptMetalShader->tFragmentLibrary = [ptDevice->tDevice  newLibraryWithSource:fragmentSource options:ptCompileOptions error:&error];
        if (ptMetalShader->tFragmentLibrary == nil)
        {
            NSLog(@"Error: failed to create Metal fragment library: %@", error);
        }
    }

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    // renderpass stuff
    const plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[tShader.tDescription.tRenderPassLayout.uIndex];

    size_t uTotalConstantSize = 0;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        uTotalConstantSize += pl__get_data_type_size(ptConstant->tType);
    }

    const plRenderPassLayout* ptRenderPassLayout = &ptDevice->sbtRenderPassLayoutsCold[ptDescription->tRenderPassLayout.uIndex];

    const uint32_t uNewResourceIndex = tHandle.uIndex;

    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptDescription->pTempConstantData;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
    }

    id<MTLFunction> vertexFunction = [ptMetalShader->tVertexLibrary newFunctionWithName:vertexEntry constantValues:ptConstantValues error:&error];
    id<MTLFunction> fragmentFunction = nil;

    if (vertexFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
    }

    if(tShader.tDescription.tPixelShader.puCode)
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
    pipelineDescriptor.rasterSampleCount = 1;

    uint32_t uCurrentColorAttachment = 0;
    const plSubpass* ptSubpass = &ptLayout->tDesc.atSubpasses[ptDescription->uSubpassIndex];
    for(uint32_t j = 0; j < ptSubpass->uRenderTargetCount; j++)
    {
        const uint32_t uTargetIndex = ptSubpass->auRenderTargets[j];
        if(pl__is_depth_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
        {
            pipelineDescriptor.depthAttachmentPixelFormat = pl__metal_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat);
            if(pl__is_stencil_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
            {
                pipelineDescriptor.stencilAttachmentPixelFormat = pipelineDescriptor.depthAttachmentPixelFormat;
            }
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
    
    ptDevice->sbtShadersCold[tHandle.uIndex] = tShader;
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

static void
pl_initialize_graphics(const plGraphicsInit* ptDesc)
{
    gptGraphics = PL_ALLOC(sizeof(plGraphics));
    memset(gptGraphics, 0, sizeof(plGraphics));
    gptDataRegistry->set_data("plGraphics", gptGraphics);
    gptGraphics->bValidationActive = ptDesc->tFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);
}

static plSurface*
pl_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));
    return ptSurface;
}

static plDevice*
pl__create_device(const plDeviceInit* ptInit)
{
    plIO* ptIOCtx = gptIOI->get_io();

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));

    ptDevice->tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    // create command queue
    gptGraphics->tCmdQueue = [ptDevice->tDevice newCommandQueue];

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
    ptHeapDescriptor.size        = PL_ARGUMENT_BUFFER_HEAP_SIZE;
    ptHeapDescriptor.type        = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
    ptHeapDescriptor.sparsePageSize = MTLSparsePageSize256;

    pl_sb_resize(ptDevice->sbtGarbage, gptGraphics->uFramesInFlight + 1);
    gptGraphics->tFence = [ptDevice->tDevice newFence];
    plTempAllocator tTempAllocator = {0};
    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {
            .tFrameBoundarySemaphore = dispatch_semaphore_create(1),
            .tDescriptorHeap = [ptDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor]
        };
        tFrame.tDescriptorHeap.label = [NSString stringWithUTF8String:pl_temp_allocator_sprintf(&tTempAllocator, "Descriptor Heap: %u", i)];
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        static char atNameBuffer[PL_MAX_NAME_LENGTH] = {0};
        pl_sprintf(atNameBuffer, "D-BUF-F%d-0", (int)i);
        tFrame.sbtDynamicBuffers[0].tMemory = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, 0, PL_DEVICE_ALLOCATION_BLOCK_SIZE, 0,atNameBuffer);
        tFrame.sbtDynamicBuffers[0].tBuffer = [(id<MTLHeap>)tFrame.sbtDynamicBuffers[0].tMemory.uHandle newBufferWithLength:PL_DEVICE_ALLOCATION_BLOCK_SIZE options:MTLResourceStorageModeShared offset:0];
        tFrame.sbtDynamicBuffers[0].tBuffer.label = [NSString stringWithUTF8String:pl_temp_allocator_sprintf(&tTempAllocator, "Dynamic Buffer: %u, 0", i)];
        
        plMetalBuffer tArgumentBuffer = {
            .tBuffer = [tFrame.tDescriptorHeap newBufferWithLength:PL_DYNAMIC_ARGUMENT_BUFFER_SIZE options:MTLResourceStorageModeShared offset:tFrame.ulDescriptorHeapOffset]
        };
        tFrame.ulDescriptorHeapOffset += PL_DYNAMIC_ARGUMENT_BUFFER_SIZE;
        tFrame.ulDescriptorHeapOffset = PL__ALIGN_UP(tFrame.ulDescriptorHeapOffset, 256);

        pl_sb_push(tFrame.sbtArgumentBuffers, tArgumentBuffer);
        pl_sb_push(ptDevice->sbFrames, tFrame);
    }
    pl_temp_allocator_free(&tTempAllocator);

    return ptDevice;
}

static plSwapchain*
pl_create_swapchain(plDevice* ptDevice, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));

    ptSwap->ptSurface = ptInit->ptSurface;
    ptSwap->ptDevice = ptDevice;
    ptSwap->uImageCount = gptGraphics->uFramesInFlight;
    ptSwap->tFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptSwap->bVSync = true;
    pl_sb_resize(ptSwap->sbtSwapchainTextureViews, gptGraphics->uFramesInFlight);

    // ptSwap->tMsaaSamples = 1;
    // if([ptDevice->tDevice supportsTextureSampleCount:8])
    //    ptSwap->tMsaaSamples = 8;

    // create main renderpass layout
    {
        uint32_t uResourceIndex = UINT32_MAX;
        if(pl_sb_size(ptDevice->sbtRenderPassLayoutFreeIndices) > 0)
            uResourceIndex = pl_sb_pop(ptDevice->sbtRenderPassLayoutFreeIndices);
        else
        {
            uResourceIndex = pl_sb_size(ptDevice->sbtRenderPassLayoutsCold);
            pl_sb_add(ptDevice->sbtRenderPassLayoutsCold);
            pl_sb_push(ptDevice->sbtRenderPassLayoutGenerations, UINT32_MAX);
            pl_sb_add(ptDevice->sbtRenderPassLayoutsHot);
        }

        plRenderPassLayoutHandle tHandle = {
            .uGeneration = ++ptDevice->sbtRenderPassLayoutGenerations[uResourceIndex],
            .uIndex = uResourceIndex
        };

        plRenderPassLayout tLayout = {
            .tDesc = {
                .atRenderTargets = {
                    {
                        .tFormat = ptSwap->tFormat,
                    }
                },
                .uSubpassCount = 1,
                .atSubpasses = {
                    {
                        .uRenderTargetCount = 1,
                        .auRenderTargets = {0}
                    },
                }
            }
        };

        ptDevice->sbtRenderPassLayoutsHot[uResourceIndex] = (plMetalRenderPassLayout){0};
        ptDevice->sbtRenderPassLayoutsCold[uResourceIndex] = tLayout;
        ptDevice->tMainRenderPassLayout = tHandle;
    }

    // create main render pass
    {
        uint32_t uResourceIndex = UINT32_MAX;
        if(pl_sb_size(ptDevice->sbtRenderPassFreeIndices) > 0)
            uResourceIndex = pl_sb_pop(ptDevice->sbtRenderPassFreeIndices);
        else
        {
            uResourceIndex = pl_sb_size(ptDevice->sbtRenderPassesCold);
            pl_sb_add(ptDevice->sbtRenderPassesCold);
            pl_sb_push(ptDevice->sbtRenderPassGenerations, UINT32_MAX);
            pl_sb_add(ptDevice->sbtRenderPassesHot);
        }

        plRenderPassHandle tHandle = {
            .uGeneration = ++ptDevice->sbtRenderPassGenerations[uResourceIndex],
            .uIndex = uResourceIndex
        };

        plRenderPass tRenderPass = {
            .tDesc = {
                .tDimensions = {gptIOI->get_io()->afMainViewportSize[0], gptIOI->get_io()->afMainViewportSize[1]},
                .tLayout = ptDevice->tMainRenderPassLayout,
                .ptSwapchain = ptSwap
            },
        };

        plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptDevice->tMainRenderPassLayout.uIndex];

        plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[uResourceIndex];

        // render pass descriptor
        for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
        {
            MTLRenderPassDescriptor* ptRenderPassDescriptor = [MTLRenderPassDescriptor new];

            ptRenderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            ptRenderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
            ptRenderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
            ptRenderPassDescriptor.colorAttachments[0].texture = gptGraphics->tCurrentDrawable.texture;
            ptRenderPassDescriptor.depthAttachment.texture = nil;

            pl_sb_push(ptMetalRenderPass->atRenderPassDescriptors[i].sbptRenderPassDescriptor, ptRenderPassDescriptor);
        }

        ptDevice->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
        ptDevice->tMainRenderPass = tHandle;
    }
    return ptSwap;
}

static void
pl_resize(plSwapchain* ptSwapchain)
{
    gptGraphics->uCurrentFrameIndex = 0;
}

static bool
pl_begin_frame(plSwapchain* ptSwapchain)
{
    pl_begin_profile_sample(__FUNCTION__);

    plDevice* ptDevice = ptSwapchain->ptDevice;

    // Wait until the inflight command buffer has completed its work
    // gptGraphics->tSwapchain.uCurrentImageIndex = gptGraphics->uCurrentFrameIndex;
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    ptFrame->uCurrentArgumentBuffer = 0;
    ptFrame->szCurrentArgumentOffset = 0;

    dispatch_semaphore_wait(ptFrame->tFrameBoundarySemaphore, DISPATCH_TIME_FOREVER);

    plIO* ptIOCtx = gptIOI->get_io();
    gptGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;

    static bool bFirstRun = true;
    if(bFirstRun == false)
    {
        pl__garbage_collect(ptDevice);
    }
    else
    {
        bFirstRun = false;
    }
    
    // get next drawable
    gptGraphics->tCurrentDrawable = [gptGraphics->pMetalLayer nextDrawable];

    if(!gptGraphics->tCurrentDrawable)
    {
        pl_end_profile_sample();
        return false;
    }

    pl_end_profile_sample();
    return true;
}

static plCommandBufferHandle
pl_begin_command_recording(plDevice* ptDevice, const plBeginCommandInfo* ptBeginInfo)
{
    plCommandBufferHandle tHandle = pl__get_new_command_buffer_handle();
    MTLCommandBufferDescriptor* ptCmdBufferDescriptor = [MTLCommandBufferDescriptor new];
    ptCmdBufferDescriptor.retainedReferences = NO;
    ptCmdBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    gptGraphics->sbtCommandBuffers[tHandle.uIndex].tCmdBuffer = [gptGraphics->tCmdQueue commandBufferWithDescriptor:ptCmdBufferDescriptor];
    // [ptCmdBufferDescriptor release];
    // char blah[32] = {0};
    // pl_sprintf(blah, "%u", gptGraphics->uCurrentFrameIndex);
    // tCmdBuffer.label = [NSString stringWithUTF8String:blah];


    if(ptBeginInfo)
    {
        gptGraphics->sbtCommandBuffers[tHandle.uIndex].tBeginInfo = *ptBeginInfo;
        for(uint32_t i = 0; i < ptBeginInfo->uWaitSemaphoreCount; i++)
        {
            if(ptDevice->sbtSemaphoresHot[ptBeginInfo->atWaitSempahores[i].uIndex].tEvent)
            {
                [gptGraphics->sbtCommandBuffers[tHandle.uIndex].tCmdBuffer encodeWaitForEvent:ptDevice->sbtSemaphoresHot[ptBeginInfo->atWaitSempahores[i].uIndex].tEvent value:ptBeginInfo->auWaitSemaphoreValues[i]];
            }
            else
            {
                [gptGraphics->sbtCommandBuffers[tHandle.uIndex].tCmdBuffer encodeWaitForEvent:ptDevice->sbtSemaphoresHot[ptBeginInfo->atWaitSempahores[i].uIndex].tSharedEvent value:ptBeginInfo->auWaitSemaphoreValues[i]];
            }
        }
    }

    gptGraphics->sbtCommandBuffers[tHandle.uIndex].ptDevice = ptDevice;
    return tHandle;
}

static void
pl_end_command_recording(plCommandBufferHandle tHandle)
{
    id<MTLCommandBuffer> tCmdBuffer = pl__get_command_buffer(tHandle)->tCmdBuffer;
    [tCmdBuffer enqueue];
}

static bool
pl_present(plCommandBufferHandle tHandle, const plSubmitInfo* ptSubmitInfo, plSwapchain* ptSwap)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tHandle);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    [ptCmdBuffer->tCmdBuffer presentDrawable:gptGraphics->tCurrentDrawable];

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            if(ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent)
            {
                [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }
    
    ptFrame->uCurrentBufferIndex = UINT32_MAX;

    __block dispatch_semaphore_t semaphore = ptFrame->tFrameBoundarySemaphore;
    [ptCmdBuffer->tCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {

        if(commandBuffer.status == MTLCommandBufferStatusError)
        {
            NSLog(@"PRESENT: %@s", commandBuffer.error);
        }
        // GPU work is complete
        // Signal the semaphore to start the CPU work
        dispatch_semaphore_signal(semaphore);
        
    }];

    [ptCmdBuffer->tCmdBuffer commit];

    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    pl__return_command_buffer_handle(tHandle);
    return true;
}

static void
pl_next_subpass(plRenderEncoderHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    ptEncoder->_uCurrentSubpass++;

    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[ptEncoder->tRenderPassHandle.uIndex];

    [ptEncoder->tEncoder updateFence:ptMetalRenderPass->tFence afterStages:MTLRenderStageFragment | MTLRenderStageVertex];
    [ptEncoder->tEncoder endEncoding];

    id<MTLRenderCommandEncoder> tNewRenderEncoder = [ptCmdBuffer->tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[ptEncoder->_uCurrentSubpass]];
    tNewRenderEncoder.label = @"subpass encoder";
    [tNewRenderEncoder waitForFence:ptMetalRenderPass->tFence beforeStages:MTLRenderStageFragment | MTLRenderStageVertex];
    ptEncoder->tEncoder = tNewRenderEncoder;
}

static plRenderEncoderHandle
pl_begin_render_pass(plCommandBufferHandle tCmdBuffer, plRenderPassHandle tPass)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tCmdBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plRenderEncoderHandle tHandle = pl__get_new_render_encoder_handle();

    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[tPass.uIndex];
    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[tPass.uIndex];
    plRenderPassLayout* ptLayout = &ptDevice->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];

    if(ptRenderPass->tDesc.ptSwapchain)
    {
        ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0].colorAttachments[0].texture = gptGraphics->tCurrentDrawable.texture;
        gptGraphics->sbtRenderEncoders[tHandle.uIndex].tEncoder = [ptCmdBuffer->tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0]];
        gptGraphics->sbtRenderEncoders[tHandle.uIndex].tEncoder.label = @"main encoder";
    }
    else
    {
        gptGraphics->sbtRenderEncoders[tHandle.uIndex].tEncoder = [ptCmdBuffer->tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[gptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0]];
        gptGraphics->sbtRenderEncoders[tHandle.uIndex].tEncoder.label = @"offscreen encoder";
        [gptGraphics->sbtRenderEncoders[tHandle.uIndex].tEncoder waitForFence:ptMetalRenderPass->tFence beforeStages:MTLRenderStageFragment | MTLRenderStageVertex];
    }

    gptGraphics->sbtRenderEncoders[tHandle.uIndex].tCommandBuffer = tCmdBuffer;
    gptGraphics->sbtRenderEncoders[tHandle.uIndex].tRenderPassHandle = tPass;
    gptGraphics->sbtRenderEncoders[tHandle.uIndex]._uCurrentSubpass = 0;
    return tHandle;
}

static void
pl_end_render_pass(plRenderEncoderHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tHandle);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plRenderPass* ptRenderPass = &ptDevice->sbtRenderPassesCold[ptEncoder->tRenderPassHandle.uIndex];
    plMetalRenderPass* ptMetalRenderPass = &ptDevice->sbtRenderPassesHot[ptEncoder->tRenderPassHandle.uIndex];
    if(ptRenderPass->tDesc.ptSwapchain == NULL)
    {
        [ptEncoder->tEncoder updateFence:ptMetalRenderPass->tFence afterStages:MTLRenderStageFragment | MTLRenderStageVertex];
    }
    [ptEncoder->tEncoder endEncoding];
    pl__return_render_encoder_handle(tHandle);
}

static void
pl_submit_command_buffer(plCommandBufferHandle tHandle, const plSubmitInfo* ptSubmitInfo)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tHandle);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {

            if(ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent)
            {
                [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
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
    pl__return_command_buffer_handle(tHandle);
}

static void
pl_submit_command_buffer_blocking(plCommandBufferHandle tHandle, const plSubmitInfo* ptSubmitInfo)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tHandle);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            if(ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent)
            {
                [ptCmdBuffer->tCmdBuffer  encodeSignalEvent:ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [ptCmdBuffer->tCmdBuffer  encodeSignalEvent:ptDevice->sbtSemaphoresHot[ptSubmitInfo->atSignalSempahores[i].uIndex].tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }

    [ptCmdBuffer->tCmdBuffer commit];
    [ptCmdBuffer->tCmdBuffer waitUntilCompleted];
    pl__return_command_buffer_handle(tHandle);
}

static plBlitEncoderHandle
pl_begin_blit_pass(plCommandBufferHandle tCmdBuffer)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tCmdBuffer);
    plBlitEncoderHandle tHandle = pl__get_new_blit_encoder_handle();
    gptGraphics->sbtBlitEncoders[tHandle.uIndex].tEncoder = [ptCmdBuffer->tCmdBuffer blitCommandEncoder];
    // plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    // [tBlitEncoder waitForFence:ptFrame->tFence];
    gptGraphics->sbtBlitEncoders[tHandle.uIndex].tCommandBuffer = tCmdBuffer;
    return tHandle;
}

static void
pl_end_blit_pass(plBlitEncoderHandle tHandle)
{
    plBlitEncoder* ptEncoder = pl__get_blit_encoder(tHandle);
    [ptEncoder->tEncoder endEncoding];
    pl__return_blit_encoder_handle(tHandle);
}

static plComputeEncoderHandle
pl_begin_compute_pass(plCommandBufferHandle tCmdBuffer)
{
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(tCmdBuffer);
    plComputeEncoderHandle tHandle = pl__get_new_compute_encoder_handle();
    gptGraphics->sbtComputeEncoders[tHandle.uIndex].tEncoder = [ptCmdBuffer->tCmdBuffer computeCommandEncoder];
    gptGraphics->sbtComputeEncoders[tHandle.uIndex].tCommandBuffer = tCmdBuffer;
    return tHandle;
}

static void
pl_end_compute_pass(plComputeEncoderHandle tHandle)
{
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tHandle);
    [ptEncoder->tEncoder endEncoding];
    pl__return_compute_encoder_handle(tHandle);
}

static void
pl_dispatch(plComputeEncoderHandle tHandle, uint32_t uDispatchCount, const plDispatch* atDispatches)
{
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tHandle);
    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        MTLSize tGridSize = MTLSizeMake(ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
        MTLSize tThreadsPerGroup = MTLSizeMake(ptDispatch->uThreadPerGroupX, ptDispatch->uThreadPerGroupY, ptDispatch->uThreadPerGroupZ);
        [ptEncoder->tEncoder dispatchThreadgroups:tGridSize threadsPerThreadgroup:tThreadsPerGroup];
    }
}

static void
pl_bind_compute_bind_groups(plComputeEncoderHandle tEncoder, plComputeShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(ptDynamicBinding)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
        [ptEncoder->tEncoder setBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:uFirst + uCount];
    }

    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        [ptEncoder->tEncoder useHeap:ptDevice->sbFrames[i].tDescriptorHeap];
    }

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
pl_bind_graphics_bind_groups(plRenderEncoderHandle tEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(ptDynamicBinding)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
        [ptEncoder->tEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:uFirst + uCount];
        [ptEncoder->tEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:uFirst + uCount];
    }

    for(uint32_t i = 0; i < uCount; i++)
    {
        plMetalBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];

        for(uint32 j = 0; j < ptBindGroup->uHeapCount; j++)
        {
            [ptEncoder->tEncoder useHeap:ptBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
        }

        for(uint32_t k = 0; k < ptBindGroup->tLayout.uTextureBindingCount; k++)
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
pl_set_viewport(plRenderEncoderHandle tEncoder, const plRenderViewport* ptViewport)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
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
pl_set_scissor_region(plRenderEncoderHandle tEncoder, const plScissor* ptScissor)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    MTLScissorRect tScissorRect = {
        .x      = (NSUInteger)(ptScissor->iOffsetX),
        .y      = (NSUInteger)(ptScissor->iOffsetY),
        .width  = (NSUInteger)(ptScissor->uWidth),
        .height = (NSUInteger)(ptScissor->uHeight)
    };
    [ptEncoder->tEncoder setScissorRect:tScissorRect];
}

static void
pl_bind_vertex_buffer(plRenderEncoderHandle tEncoder, plBufferHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    [ptEncoder->tEncoder setVertexBuffer:ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer
        offset:0
        atIndex:4];
}

static void
pl_draw(plRenderEncoderHandle tEncoder, uint32_t uCount, const plDraw* atDraws)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);

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
pl_draw_indexed(plRenderEncoderHandle tEncoder, uint32_t uCount, const plDrawIndex* atDraws)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
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
pl_bind_shader(plRenderEncoderHandle tEncoder, plShaderHandle tHandle)
{
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
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
pl_bind_compute_shader(plComputeEncoderHandle tEncoder, plComputeShaderHandle tHandle)
{
    plComputeEncoder* ptEncoder = pl__get_compute_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plMetalComputeShader* ptMetalShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    [ptEncoder->tEncoder setComputePipelineState:ptMetalShader->tPipelineState];
}

static void
pl_draw_stream(plRenderEncoderHandle tEncoder, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(__FUNCTION__);
    plRenderEncoder* ptEncoder = pl__get_render_encoder(tEncoder);
    plCommandBuffer* ptCmdBuffer = pl__get_command_buffer(ptEncoder->tCommandBuffer);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        [ptEncoder->tEncoder useHeap:ptDevice->sbFrames[i].tDescriptorHeap stages:MTLRenderStageVertex | MTLRenderStageFragment];
    }

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

        const uint32_t uTokens = pl_sb_size(ptStream->sbtStream);
        uint32_t uCurrentStreamIndex = 0;
        uint32_t uTriangleCount = 0;
        uint32_t uIndexBuffer = 0;
        uint32_t uIndexBufferOffset = 0;
        uint32_t uVertexBufferOffset = 0;
        uint32_t uDynamicBufferOffset = 0;
        uint32_t uInstanceStart = 0;
        uint32_t uInstanceCount = 1;
        id<MTLDepthStencilState> tCurrentDepthStencilState = nil;

        uint32_t uDynamicSlot = UINT32_MAX;
        while(uCurrentStreamIndex < uTokens)
        {
            const uint32_t uDirtyMask = ptStream->sbtStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                const plShader* ptShader= &ptDevice->sbtShadersCold[ptStream->sbtStream[uCurrentStreamIndex]];
                plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[ptStream->sbtStream[uCurrentStreamIndex]];
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
                uDynamicSlot = ptShader->tDescription.uBindGroupLayoutCount;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                uDynamicBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [ptEncoder->tEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureBindingCount; k++)
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
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [ptEncoder->tEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureBindingCount; k++)
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
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                
                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [ptEncoder->tEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureBindingCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptMetalBindGroup->atTextureBindings[k];
                    [ptEncoder->tEncoder useResource:ptDevice->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment]; 
                }

                [ptEncoder->tEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:2];
                [ptEncoder->tEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:2];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
            {
                
                [ptEncoder->tEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer offset:0 atIndex:uDynamicSlot];
                [ptEncoder->tEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer offset:0 atIndex:uDynamicSlot];

                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                [ptEncoder->tEncoder setVertexBufferOffset:uDynamicBufferOffset atIndex:uDynamicSlot];
                [ptEncoder->tEncoder setFragmentBufferOffset:uDynamicBufferOffset atIndex:uDynamicSlot];
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
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
            {
                [ptEncoder->tEncoder setVertexBuffer:ptDevice->sbtBuffersHot[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer
                    offset:0
                    atIndex:4];
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
                    indexBuffer:ptDevice->sbtBuffersHot[uIndexBuffer].tBuffer
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
    pl_sb_free(ptDevice->sbtSemaphoresHot);

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptDevice->sbFrames[i];
        pl_sb_free(ptFrame->sbtDynamicBuffers);
        pl_sb_free(ptFrame->sbtArgumentBuffers);
    }
    pl_sb_free(ptDevice->sbFrames);

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
    pl_begin_profile_sample(__FUNCTION__);
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtRenderPasses); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPasses[i].uIndex;
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
        const uint32_t iResourceIndex = ptGarbage->sbtRenderPassLayouts[i].uIndex;
        plMetalRenderPassLayout* ptMetalResource = &ptDevice->sbtRenderPassLayoutsHot[iResourceIndex];
        pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtShaders[i].uIndex;
        plShader* ptResource = &ptDevice->sbtShadersCold[iResourceIndex];

        plMetalShader* ptVariantMetalResource = &ptDevice->sbtShadersHot[iResourceIndex];
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
        pl_sb_push(ptDevice->sbtShaderFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
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
        const uint32_t iBindGroupIndex = ptGarbage->sbtBindGroups[i].uIndex;
        plMetalBindGroup* ptMetalResource = &ptDevice->sbtBindGroupsHot[iBindGroupIndex];
        [ptMetalResource->tShaderArgumentBuffer release];
        ptMetalResource->tShaderArgumentBuffer = nil;
        pl_sb_push(ptDevice->sbtBindGroupFreeIndices, iBindGroupIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtSamplers); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtSamplers[i].uIndex;
        plMetalSampler* ptMetalSampler = &ptDevice->sbtSamplersHot[iResourceIndex];
        [ptMetalSampler->tSampler release];
        ptMetalSampler->tSampler = nil;
        pl_sb_push(ptDevice->sbtSamplerFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint32_t uTextureIndex = ptGarbage->sbtTextures[i].uIndex;
        plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[uTextureIndex];
        [ptMetalTexture->tTexture release];
        ptMetalTexture->tTexture = nil;
        pl_sb_push(ptDevice->sbtTextureFreeIndices, uTextureIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBuffers); i++)
    {
        const uint32_t iBufferIndex = ptGarbage->sbtBuffers[i].uIndex;
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
    pl_end_profile_sample();
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

    ptDevice->sbtBufferGenerations[tHandle.uIndex]++;
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
    ptDevice->sbtTextureGenerations[tHandle.uIndex]++;

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
    ptDevice->sbtBindGroupGenerations[tHandle.uIndex]++;
    pl_sb_push(ptDevice->sbtBindGroupFreeIndices, tHandle.uIndex);

    plMetalBindGroup* ptMetalResource = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    [ptMetalResource->tShaderArgumentBuffer release];
    ptMetalResource->tShaderArgumentBuffer = nil;
}

static void
pl_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    ptDevice->sbtRenderPassGenerations[tHandle.uIndex]++;
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
    ptDevice->sbtRenderPassLayoutGenerations[tHandle.uIndex]++;
    pl_sb_push(ptDevice->sbtRenderPassLayoutFreeIndices, tHandle.uIndex);
}

static void
pl_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    ptDevice->sbtShaderGenerations[tHandle.uIndex]++;

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
    ptDevice->sbtComputeShaderGenerations[tHandle.uIndex]++;

    plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[tHandle.uIndex];
    plMetalComputeShader* ptVariantMetalResource = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    [ptVariantMetalResource->tPipelineState release];
    ptVariantMetalResource->tPipelineState = nil;
    pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, tHandle.uIndex);
}
