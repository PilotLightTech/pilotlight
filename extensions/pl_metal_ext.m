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

const plFileI* gptFile    = NULL;
const plIOI*    gptIO = NULL;
const plThreadsI*  gptThread = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal structs & types
//-----------------------------------------------------------------------------

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

typedef struct _plMetalFrameBuffer
{
    MTLRenderPassDescriptor** sbptRenderPassDescriptor;
} plMetalFrameBuffer;

typedef struct _plMetalRenderPass
{
    plMetalFrameBuffer atRenderPassDescriptors[PL_FRAMES_IN_FLIGHT];
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

typedef struct _plGraphicsMetal
{
    plTempAllocator     tTempAllocator;
    id<MTLCommandQueue> tCmdQueue;
    CAMetalLayer*       pMetalLayer;
    id<MTLFence>        tFence;
    
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
    
    // per frame
    id<CAMetalDrawable>         tCurrentDrawable;
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
static void                  pl__garbage_collect(plGraphics* ptGraphics);

static plDeviceMemoryAllocation pl_allocate_memory(plDevice* ptDevice, size_t ulSize, plMemoryMode tMemoryMode, uint32_t uTypeFilter, const char* pcName);
static void pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plFrameContext*
pl__get_frame_resources(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    return &ptMetalGraphics->sbFrames[ptGraphics->uCurrentFrameIndex];
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
                    .tFormat = ptGraphics->tSwapchain.tFormat,
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

    ptMetalGraphics->sbtRenderPassLayoutsHot[uResourceIndex] = (plMetalRenderPassLayout){0};
    ptGraphics->sbtRenderPassLayoutsCold[uResourceIndex] = tLayout;
    return tHandle;
}

static void
pl_update_render_pass_attachments(plDevice* ptDevice, plRenderPassHandle tHandle, plVec2 tDimensions, const plRenderPassAttachments* ptAttachments)
{

    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plRenderPass* ptRenderPass = &ptGraphics->sbtRenderPassesCold[tHandle.uIndex];
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[tHandle.uIndex];
    ptRenderPass->tDesc.tDimensions = tDimensions;

    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptRenderPass->tDesc.tLayout.uIndex];

    for(uint32_t uFrameIndex = 0; uFrameIndex < PL_FRAMES_IN_FLIGHT; uFrameIndex++)
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
                    ptRenderPassDescriptor.depthAttachment.texture = ptMetalGraphics->sbtTexturesHot[uTextureIndex].tTexture;
                    ptRenderPassDescriptor.depthAttachment.slice = ptGraphics->sbtTexturesCold[uTextureIndex].tView.uBaseLayer;
                    if(pl__is_stencil_format(ptLayout->tDesc.atRenderTargets[uTargetIndex].tFormat))
                    {
                        ptRenderPassDescriptor.stencilAttachment.texture = ptMetalGraphics->sbtTexturesHot[uTextureIndex].tTexture;
                    }
                }
                else
                {
                    ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].texture = ptMetalGraphics->sbtTexturesHot[uTextureIndex].tTexture;
                    uCurrentColorAttachment++;
                }
            }
        }
    }
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
            .tDimensions = {gptIO->get_io()->afMainViewportSize[0], gptIO->get_io()->afMainViewportSize[1]},
            .tLayout = ptGraphics->tMainRenderPassLayout
        },
        .bSwapchain = true
    };

    plRenderPassLayout* ptLayout = &ptGraphics->sbtRenderPassLayoutsCold[ptGraphics->tMainRenderPassLayout.uIndex];

    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[uResourceIndex];

    // render pass descriptor
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        MTLRenderPassDescriptor* ptRenderPassDescriptor = [MTLRenderPassDescriptor new];

        ptRenderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        ptRenderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        ptRenderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        ptRenderPassDescriptor.colorAttachments[0].texture = ptMetalGraphics->tCurrentDrawable.texture;
        ptRenderPassDescriptor.depthAttachment.texture = nil;

        pl_sb_push(ptMetalRenderPass->atRenderPassDescriptors[i].sbptRenderPassDescriptor, ptRenderPassDescriptor);
    }

    ptGraphics->sbtRenderPassesCold[uResourceIndex] = tRenderPass;
    ptGraphics->tMainRenderPass = tHandle;
}

static plRenderPassHandle
pl_create_render_pass(plDevice* ptDevice, const plRenderPassDescription* ptDesc, const plRenderPassAttachments* ptAttachments)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;

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
    ptMetalRenderPass->tFence = [ptMetalDevice->tDevice newFence];

    

    // subpasses
    for(uint32_t uFrameIndex = 0; uFrameIndex < PL_FRAMES_IN_FLIGHT; uFrameIndex++)
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
                        ptRenderPassDescriptor.stencilAttachment.texture = ptMetalGraphics->sbtTexturesHot[uTextureIndex].tTexture;
                    }
                    ptRenderPassDescriptor.depthAttachment.texture = ptMetalGraphics->sbtTexturesHot[uTextureIndex].tTexture;
                    ptRenderPassDescriptor.depthAttachment.slice = ptGraphics->sbtTexturesCold[uTextureIndex].tView.uBaseLayer;
                    ptLayout->tDesc.atSubpasses[i]._bHasDepth = true;
                }
                else
                {
                    const uint32_t uTargetIndexOriginal = uTargetIndex;
                    if(ptLayout->tDesc.atSubpasses[i]._bHasDepth)
                        uTargetIndex--;
                    ptRenderPassDescriptor.colorAttachments[uCurrentColorAttachment].texture = ptMetalGraphics->sbtTexturesHot[uTextureIndex].tTexture;

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
            destinationLevel:ptRegions[i].uMipLevel 
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
            gptThread->sleep_thread(1);
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

    MTLResourceOptions tStorageMode = MTLResourceStorageModePrivate;
    if(ptDesc->tUsage & PL_BUFFER_USAGE_STAGING)
    {
        tStorageMode = MTLResourceStorageModeShared;
    }

    MTLSizeAndAlign tSizeAndAlign = [ptMetalDevice->tDevice heapBufferSizeAndAlignWithLength:ptDesc->uByteSize options:tStorageMode];
    tBuffer.tMemoryRequirements.ulSize = tSizeAndAlign.size;
    tBuffer.tMemoryRequirements.ulAlignment = tSizeAndAlign.align;
    tBuffer.tMemoryRequirements.uMemoryTypeBits = 0;

    plMetalBuffer tMetalBuffer = {
        0
    };
    ptMetalGraphics->sbtBuffersHot[uBufferIndex] = tMetalBuffer;
    ptGraphics->sbtBuffersCold[uBufferIndex] = tBuffer;
    return tHandle;
}

static void
pl_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plBuffer* ptBuffer = &ptGraphics->sbtBuffersCold[tHandle.uIndex];
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plMetalBuffer* ptMetalBuffer = &ptMetalGraphics->sbtBuffersHot[tHandle.uIndex];

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
pl_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, const char* pcName)
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

    MTLSizeAndAlign tSizeAndAlign = [ptMetalDevice->tDevice heapTextureSizeAndAlignWithDescriptor:ptTextureDescriptor];
    tTexture.tMemoryRequirements.ulAlignment = tSizeAndAlign.align;
    tTexture.tMemoryRequirements.ulSize = tSizeAndAlign.size;
    tTexture.tMemoryRequirements.uMemoryTypeBits = 0;
    plMetalTexture tMetalTexture = {
        .ptTextureDescriptor = ptTextureDescriptor,
        .bOriginalView = true
    };
    ptMetalGraphics->sbtTexturesHot[uTextureIndex] = tMetalTexture;
    ptGraphics->sbtTexturesCold[uTextureIndex] = tTexture;
    return tHandle;
}

static plSamplerHandle
pl_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptGraphics->sbtSamplerFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptGraphics->sbtSamplerFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptGraphics->sbtSamplersCold);
        pl_sb_add(ptGraphics->sbtSamplersCold);
        pl_sb_push(ptGraphics->sbtSamplerGenerations, UINT32_MAX);
        pl_sb_add(ptMetalGraphics->sbtSamplersHot);
    }

    plSamplerHandle tHandle = {
        .uGeneration = ++ptGraphics->sbtSamplerGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };

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
        .tSampler = [ptMetalDevice->tDevice newSamplerStateWithDescriptor:samplerDesc]
    };

    ptMetalGraphics->sbtSamplersHot[uResourceIndex] = tMetalSampler;
    ptGraphics->sbtSamplersCold[uResourceIndex] = tSampler;
    return tHandle;
}

static plBindGroupHandle
pl_get_temporary_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    if(pcName == NULL)
        pcName = "unnamed temporary bind group";

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

    NSUInteger argumentBufferLength = sizeof(uint64_t) * (ptLayout->uTextureBindingCount + ptLayout->uBufferBindingCount + ptLayout->uSamplerBindingCount);

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

    ptMetalGraphics->sbtBindGroupsHot[uBindGroupIndex] = tMetalBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    pl_queue_bind_group_for_deletion(ptDevice, tHandle);
    return tHandle;
}

static plBindGroupHandle
pl_create_bind_group(plDevice* ptDevice, const plBindGroupLayout* ptLayout, const char* pcName)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    if(pcName == NULL)
        pcName = "unnamed bind group";

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
    MTLSizeAndAlign tSizeAlign = [ptMetalDevice->tDevice heapBufferSizeAndAlignWithLength:argumentBufferLength options:MTLResourceStorageModeShared];

    PL_ASSERT(ptFrame->ulDescriptorHeapOffset + tSizeAlign.size < PL_DEVICE_ALLOCATION_BLOCK_SIZE);

    plMetalBindGroup tMetalBindGroup = {
        .tShaderArgumentBuffer = [ptFrame->tDescriptorHeap newBufferWithLength:tSizeAlign.size options:MTLResourceStorageModeShared offset:ptFrame->ulDescriptorHeapOffset]
    };
    tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:pcName];
    [tMetalBindGroup.tShaderArgumentBuffer retain];
    ptFrame->ulDescriptorHeapOffset += tSizeAlign.size;
    ptFrame->ulDescriptorHeapOffset = PL__ALIGN_UP(ptFrame->ulDescriptorHeapOffset, tSizeAlign.align);

    ptMetalGraphics->sbtBindGroupsHot[uBindGroupIndex] = tMetalBindGroup;
    ptGraphics->sbtBindGroupsCold[uBindGroupIndex] = tBindGroup;
    return tHandle;
}

static void
pl_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[tHandle.uIndex];
    plBindGroup* ptBindGroup = &ptGraphics->sbtBindGroupsCold[tHandle.uIndex];


    ptMetalBindGroup->uHeapCount = 0;
    const char* pcDescriptorStart = ptMetalBindGroup->tShaderArgumentBuffer.contents;

    uint64_t* pulDescriptorStart = (uint64_t*)&pcDescriptorStart[ptMetalBindGroup->uOffset];

    for(uint32_t i = 0; i < ptData->uBufferCount; i++)
    {
        const plBindGroupUpdateBufferData* ptUpdate = &ptData->atBuffers[i];
        plMetalBuffer* ptMetalBuffer = &ptMetalGraphics->sbtBuffersHot[ptUpdate->tBuffer.uIndex];
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
        plTexture* ptTexture = &ptGraphics->sbtTexturesCold[ptUpdate->tTexture.uIndex];
        plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[ptUpdate->tTexture.uIndex];
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
        plMetalSampler* ptMetalSampler = &ptMetalGraphics->sbtSamplersHot[ptUpdate->tSampler.uIndex];
        MTLResourceID* pptDestination = (MTLResourceID*)&pulDescriptorStart[ptUpdate->uSlot];
        *pptDestination = ptMetalSampler->tSampler.gpuResourceID;
        ptMetalBindGroup->atSamplerBindings[i] = ptUpdate->tSampler;
    }
}

static void
pl_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptDevice->_pInternalData;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;

    plTexture* ptTexture = &ptGraphics->sbtTexturesCold[tHandle.uIndex];
    ptTexture->tMemoryAllocation = *ptAllocation;
    plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[tHandle.uIndex];

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
        if(pl_sb_size(ptGraphics->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptGraphics->sbtFreeDrawBindGroups);
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
        pl_update_bind_group(&ptGraphics->tDevice, ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }
}

static plTextureHandle
pl_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc, const char* pcName)
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

    plTexture tTexture = {
        .tDesc = ptGraphics->sbtTexturesCold[ptViewDesc->tTexture.uIndex].tDesc,
        .tView = *ptViewDesc
    };

    plTexture* ptTexture = pl__get_texture(ptDevice, ptViewDesc->tTexture);
    plMetalTexture* ptOldMetalTexture = &ptMetalGraphics->sbtTexturesHot[ptViewDesc->tTexture.uIndex];
    plMetalTexture* ptNewMetalTexture = &ptMetalGraphics->sbtTexturesHot[uTextureIndex];
    ptNewMetalTexture->bOriginalView = false;

    if(ptTexture->tDesc.tUsage & PL_TEXTURE_USAGE_SAMPLED)
    {
        if(pl_sb_size(ptGraphics->sbtFreeDrawBindGroups) == 0)
        {
            const plBindGroupLayout tDrawingBindGroup = {
                .uTextureBindingCount  = 1,
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            };
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_create_bind_group(ptDevice, &tDrawingBindGroup, "draw binding");
        }
        else
        {
            ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup = pl_sb_pop(ptGraphics->sbtFreeDrawBindGroups);
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
        pl_update_bind_group(&ptGraphics->tDevice, ptGraphics->sbtTexturesCold[tHandle.uIndex]._tDrawBindGroup, &tBGData);
    }

    // MTLTextureType tTextureType = MTLTextureType2D;

    // if(tTexture.tDesc.tType == PL_TEXTURE_TYPE_2D)
    //     tTextureType = MTLTextureType2D;
    // else if(tTexture.tDesc.tType == PL_TEXTURE_TYPE_CUBE)
    //     tTextureType = MTLTextureTypeCube;
    // else if(tTexture.tDesc.tType == PL_TEXTURE_TYPE_2D_ARRAY)
    //     tTextureType = MTLTextureType2DArray;
    //     // tTextureType = MTLTextureType2D;
    // else
    // {
    //     PL_ASSERT(false && "unsupported texture type");
    // }

    // NSRange tLevelRange = {
    //     .length = ptViewDesc->uMips == 0 ? ptTexture->tDesc.uMips - ptViewDesc->uBaseMip : ptViewDesc->uMips,
    //     .location = ptViewDesc->uBaseMip
    // };

    // NSRange tSliceRange = {
    //     .length = ptViewDesc->uLayerCount,
    //     .location = ptViewDesc->uBaseLayer
    // };

    // NSRange tLevelRange = {
    //     .length = ptTexture->tView.uMips,
    //     .location = ptTexture->tView.uBaseMip
    // };

    // NSRange tSliceRange = {
    //     .length = ptTexture->tView.uLayerCount,
    //     .location = ptTexture->tView.uBaseLayer
    // };

    // plMetalTexture tMetalTexture = {
    //     .tTexture = [ptOldMetalTexture->tTexture newTextureViewWithPixelFormat:pl__metal_format(ptViewDesc->tFormat) 
    //         textureType:tTextureType
    //         levels:tLevelRange
    //         slices:tSliceRange],
    //     .tHeap = ptOldMetalTexture->tHeap
    // };
    // if(pcName == NULL)
    //     pcName = "unnamed texture";
    // tMetalTexture.tTexture.label = [NSString stringWithUTF8String:pcName];
    ptNewMetalTexture->tTexture = ptOldMetalTexture->tTexture;
    ptNewMetalTexture->tHeap = ptOldMetalTexture->tHeap;

    ptGraphics->sbtTexturesCold[uTextureIndex] = tTexture;
    return tHandle;
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

    NSString* entryFunc = [NSString stringWithUTF8String:tShader.tDescription.pcShaderEntryFunc];

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
        .tPipelineState = [ptMetalDevice->tDevice newComputePipelineStateWithFunction:computeFunction error:&error]
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);

    ptMetalShader->tPipelineState = tMetalShader.tPipelineState;
    ptGraphics->sbtComputeShadersCold[uResourceIndex] = tShader;
    return tHandle;
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

    NSString* vertexEntry = [NSString stringWithUTF8String:tShader.tDescription.pcVertexShaderEntryFunc];
    NSString* fragmentEntry = [NSString stringWithUTF8String:tShader.tDescription.pcPixelShaderEntryFunc];

    // vertex layout
    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.layouts[0].stride = ptDescription->tVertexBufferBinding.uByteStride;

    uint32_t uCurrentAttributeCount = 0;
    for(uint32_t i = 0; i < PL_MAX_VERTEX_ATTRIBUTES; i++)
    {
        if(ptDescription->tVertexBufferBinding.atAttributes[i].tFormat == PL_FORMAT_UNKNOWN)
            break;
        vertexDescriptor.attributes[i].bufferIndex = 0;
        vertexDescriptor.attributes[i].offset = ptDescription->tVertexBufferBinding.atAttributes[i].uByteOffset;
        vertexDescriptor.attributes[i].format = pl__metal_vertex_format(ptDescription->tVertexBufferBinding.atAttributes[i].tFormat);
        uCurrentAttributeCount++;
    }

    uint32_t uVertShaderSize0 = 0u;
    uint32_t uPixelShaderSize0 = 0u;

    gptFile->read(tShader.tDescription.pcVertexShader, &uVertShaderSize0, NULL, "rb");
    gptFile->read(tShader.tDescription.pcPixelShader, &uPixelShaderSize0, NULL, "rb");

    char* vertexShaderCode = pl_temp_allocator_alloc(&ptMetalGraphics->tTempAllocator, uVertShaderSize0 + 1);
    char* pixelShaderCode  = pl_temp_allocator_alloc(&ptMetalGraphics->tTempAllocator, uPixelShaderSize0 + 1);
    memset(vertexShaderCode, 0, uVertShaderSize0 + 1);
    memset(pixelShaderCode, 0, uPixelShaderSize0 + 1);


    gptFile->read(tShader.tDescription.pcVertexShader, &uVertShaderSize0, vertexShaderCode, "rb");
    gptFile->read(tShader.tDescription.pcPixelShader, &uPixelShaderSize0, pixelShaderCode, "rb");

    // prepare preprocessor defines
    MTLCompileOptions* ptCompileOptions = [MTLCompileOptions new];
    ptCompileOptions.fastMathEnabled = false;

    // compile shader source
    NSError* error = nil;
    NSString* vertexSource = [NSString stringWithUTF8String:vertexShaderCode];
    ptMetalShader->tVertexLibrary = [ptMetalDevice->tDevice  newLibraryWithSource:vertexSource options:ptCompileOptions error:&error];
    if (ptMetalShader->tVertexLibrary == nil)
    {
        NSLog(@"Error: failed to create Metal vertex library: %@", error);
    }

    NSString* fragmentSource = [NSString stringWithUTF8String:pixelShaderCode];
    ptMetalShader->tFragmentLibrary = [ptMetalDevice->tDevice  newLibraryWithSource:fragmentSource options:ptCompileOptions error:&error];
    if (ptMetalShader->tFragmentLibrary == nil)
    {
        NSLog(@"Error: failed to create Metal fragment library: %@", error);
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

    const uint32_t uNewResourceIndex = uResourceIndex;

    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptDescription->pTempConstantData;
    for(uint32_t i = 0; i < tShader.tDescription.uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &tShader.tDescription.atConstants[i];
        [ptConstantValues setConstantValue:&pcConstantData[ptConstant->uOffset] type:pl__metal_data_type(ptConstant->tType) atIndex:ptConstant->uID];
    }

    id<MTLFunction> vertexFunction = [ptMetalShader->tVertexLibrary newFunctionWithName:vertexEntry constantValues:ptConstantValues error:&error];
    id<MTLFunction> fragmentFunction = [ptMetalShader->tFragmentLibrary newFunctionWithName:fragmentEntry constantValues:ptConstantValues error:&error];

    if (vertexFunction == nil || fragmentFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
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
        .tDepthStencilState   = [ptMetalDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor],
        .tRenderPipelineState = [ptMetalDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error],
        .tCullMode            = pl__metal_cull(ptDescription->tGraphicsState.ulCullMode)
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);
    
    ptGraphics->sbtShadersCold[uNewResourceIndex] = tShader;
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
    ptData->ptDevice->ptGraphics->szHostMemoryInUse += ulSize;
    return tAllocation;
}

static void
pl_free_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;
    plDeviceMemoryAllocation tBlock = {.uHandle = ptAllocation->uHandle};
    pl_free_memory(ptData->ptDevice, &tBlock);
    ptData->ptDevice->ptGraphics->szHostMemoryInUse -= ptAllocation->ulSize;
    ptAllocation->uHandle = 0;
    ptAllocation->ulSize = 0;
    ptAllocation->ulOffset = 0;
}

static void
pl_initialize_graphics(plWindow* ptWindow, const plGraphicsDesc* ptDesc, plGraphics* ptGraphics)
{
    ptGraphics->bValidationActive = ptDesc->bEnableValidation;
    plIO* ptIOCtx = gptIO->get_io();

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

    ptGraphics->uFramesInFlight = PL_FRAMES_IN_FLIGHT;
    ptGraphics->tSwapchain.uImageCount = PL_FRAMES_IN_FLIGHT;
    ptGraphics->tSwapchain.tFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptGraphics->tSwapchain.bVSync = true;
    pl_sb_resize(ptGraphics->tSwapchain.sbtSwapchainTextureViews, PL_FRAMES_IN_FLIGHT);

    // create command queue
    ptMetalGraphics->tCmdQueue = [ptMetalDevice->tDevice newCommandQueue];

    // ptGraphics->tSwapchain.tMsaaSamples = 1;
    // if([ptMetalDevice->tDevice supportsTextureSampleCount:8])
    //    ptGraphics->tSwapchain.tMsaaSamples = 8;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~device memory allocators~~~~~~~~~~~~~~~~~~~~~~~~~

    static plInternalDeviceAllocatorData tAllocatorData = {0};
    static plDeviceMemoryAllocatorI tAllocator = {0};
    tAllocatorData.ptAllocator = &tAllocator;
    tAllocatorData.ptDevice = &ptGraphics->tDevice;
    tAllocator.allocate = pl_allocate_staging_dynamic;
    tAllocator.free = pl_free_staging_dynamic;
    tAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tAllocatorData;
    ptGraphics->tDevice.ptDynamicAllocator = &tAllocator;
    plDeviceMemoryAllocatorI* ptDynamicAllocator = &tAllocator;

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.storageMode = MTLStorageModeShared;
    ptHeapDescriptor.size        = PL_ARGUMENT_BUFFER_HEAP_SIZE;
    ptHeapDescriptor.type        = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;
    ptHeapDescriptor.sparsePageSize = MTLSparsePageSize256;

    pl_sb_resize(ptGraphics->sbtGarbage, ptGraphics->uFramesInFlight + 1);
    ptMetalGraphics->tFence = [ptMetalDevice->tDevice newFence];
    plTempAllocator tTempAllocator = {0};
    for(uint32_t i = 0; i < ptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {
            .tFrameBoundarySemaphore = dispatch_semaphore_create(1),
            .tDescriptorHeap = [ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor]
        };
        tFrame.tDescriptorHeap.label = [NSString stringWithUTF8String:pl_temp_allocator_sprintf(&tTempAllocator, "Descriptor Heap: %u", i)];
        pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        static char atNameBuffer[PL_MAX_NAME_LENGTH] = {0};
        pl_sprintf(atNameBuffer, "D-BUF-F%d-0", (int)i);
        tFrame.sbtDynamicBuffers[0].tMemory = ptGraphics->tDevice.ptDynamicAllocator->allocate(ptGraphics->tDevice.ptDynamicAllocator->ptInst, 0, PL_DEVICE_ALLOCATION_BLOCK_SIZE, 0,atNameBuffer);
        tFrame.sbtDynamicBuffers[0].tBuffer = [(id<MTLHeap>)tFrame.sbtDynamicBuffers[0].tMemory.uHandle newBufferWithLength:PL_DEVICE_ALLOCATION_BLOCK_SIZE options:MTLResourceStorageModeShared offset:0];
        tFrame.sbtDynamicBuffers[0].tBuffer.label = [NSString stringWithUTF8String:pl_temp_allocator_sprintf(&tTempAllocator, "Dynamic Buffer: %u, 0", i)];
        
        plMetalBuffer tArgumentBuffer = {
            .tBuffer = [tFrame.tDescriptorHeap newBufferWithLength:PL_DYNAMIC_ARGUMENT_BUFFER_SIZE options:MTLResourceStorageModeShared offset:tFrame.ulDescriptorHeapOffset]
        };
        tFrame.ulDescriptorHeapOffset += PL_DYNAMIC_ARGUMENT_BUFFER_SIZE;
        tFrame.ulDescriptorHeapOffset = PL__ALIGN_UP(tFrame.ulDescriptorHeapOffset, 256);

        pl_sb_push(tFrame.sbtArgumentBuffers, tArgumentBuffer);
        pl_sb_push(ptMetalGraphics->sbFrames, tFrame);
    }
    pl_temp_allocator_free(&tTempAllocator);

    pl_create_main_render_pass_layout(&ptGraphics->tDevice);
    pl_create_main_render_pass(&ptGraphics->tDevice);
}

static void
pl_resize(plGraphics* ptGraphics)
{
    ptGraphics->uCurrentFrameIndex = 0;
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
    ptFrame->szCurrentArgumentOffset = 0;

    dispatch_semaphore_wait(ptFrame->tFrameBoundarySemaphore, DISPATCH_TIME_FOREVER);

    plIO* ptIOCtx = gptIO->get_io();
    ptMetalGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;

    static bool bFirstRun = true;
    if(bFirstRun == false)
    {
        pl__garbage_collect(ptGraphics);
    }
    else
    {
        bFirstRun = false;
    }
    
    // get next drawable
    ptMetalGraphics->tCurrentDrawable = [ptMetalGraphics->pMetalLayer nextDrawable];

    if(!ptMetalGraphics->tCurrentDrawable)
    {
        pl_end_profile_sample();
        return false;
    }

    pl_end_profile_sample();
    return true;
}

static plCommandBuffer
pl_begin_command_recording(plGraphics* ptGraphics, const plBeginCommandInfo* ptBeginInfo)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;

    MTLCommandBufferDescriptor* ptCmdBufferDescriptor = [MTLCommandBufferDescriptor new];
    ptCmdBufferDescriptor.retainedReferences = NO;
    ptCmdBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    id<MTLCommandBuffer> tCmdBuffer = [ptMetalGraphics->tCmdQueue commandBufferWithDescriptor:ptCmdBufferDescriptor];
    // [ptCmdBufferDescriptor release];
    char blah[32] = {0};
    pl_sprintf(blah, "%u", ptGraphics->uCurrentFrameIndex);
    tCmdBuffer.label = [NSString stringWithUTF8String:blah];
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

    __block dispatch_semaphore_t semaphore = ptFrame->tFrameBoundarySemaphore;
    [tCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {

        if(commandBuffer.status == MTLCommandBufferStatusError)
        {
            NSLog(@"PRESENT: %@s", commandBuffer.error);
        }
        // GPU work is complete
        // Signal the semaphore to start the CPU work
        dispatch_semaphore_signal(semaphore);
        
    }];

    [tCmdBuffer commit];

    ptGraphics->uCurrentFrameIndex = (ptGraphics->uCurrentFrameIndex + 1) % ptGraphics->uFramesInFlight;
    return true;
}

static void
pl_next_subpass(plRenderEncoder* ptEncoder)
{
    ptEncoder->_uCurrentSubpass++;
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[ptEncoder->tRenderPassHandle.uIndex];
    id<MTLRenderCommandEncoder> tRenderEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;

    [tRenderEncoder updateFence:ptMetalRenderPass->tFence afterStages:MTLRenderStageFragment | MTLRenderStageVertex];
    [tRenderEncoder endEncoding];



    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    id<MTLRenderCommandEncoder> tNewRenderEncoder = [tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[ptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[ptEncoder->_uCurrentSubpass]];
    tNewRenderEncoder.label = @"subpass encoder";
    [tNewRenderEncoder waitForFence:ptMetalRenderPass->tFence beforeStages:MTLRenderStageFragment | MTLRenderStageVertex];
    ptEncoder->_pInternal = tNewRenderEncoder;
    
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
        ptMetalRenderPass->atRenderPassDescriptors[ptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0].colorAttachments[0].texture = ptMetalGraphics->tCurrentDrawable.texture;
        tRenderEncoder = [tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[ptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0]];
        tRenderEncoder.label = @"main encoder";
    }
    else
    {
        tRenderEncoder = [tCmdBuffer renderCommandEncoderWithDescriptor:ptMetalRenderPass->atRenderPassDescriptors[ptGraphics->uCurrentFrameIndex].sbptRenderPassDescriptor[0]];
        tRenderEncoder.label = @"offscreen encoder";
        [tRenderEncoder waitForFence:ptMetalRenderPass->tFence beforeStages:MTLRenderStageFragment | MTLRenderStageVertex];
    }

    plRenderEncoder tEncoder = {
        .ptGraphics        = ptGraphics,
        .tCommandBuffer    = *ptCmdBuffer,
        ._uCurrentSubpass  = 0,
        ._pInternal        = tRenderEncoder,
        .tRenderPassHandle = tPass
    };
    
    return tEncoder;
}

static void
pl_end_render_pass(plRenderEncoder* ptEncoder)
{
    
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptEncoder->ptGraphics->_pInternalData;
    plRenderPass* ptRenderPass = &ptEncoder->ptGraphics->sbtRenderPassesCold[ptEncoder->tRenderPassHandle.uIndex];
    plMetalRenderPass* ptMetalRenderPass = &ptMetalGraphics->sbtRenderPassesHot[ptEncoder->tRenderPassHandle.uIndex];
    id<MTLRenderCommandEncoder> tRenderEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;
    if(!ptRenderPass->bSwapchain)
    {
        [tRenderEncoder updateFence:ptMetalRenderPass->tFence afterStages:MTLRenderStageFragment | MTLRenderStageVertex];
    }
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

    [tCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {

        if(commandBuffer.status == MTLCommandBufferStatusError)
        {
            NSLog(@"SECONDARY: %@s", commandBuffer.error);
        }
        
    }];

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
pl_dispatch(plComputeEncoder* ptEncoder, uint32_t uDispatchCount, const plDispatch* atDispatches)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    id<MTLComputeCommandEncoder> tComputeEncoder = (id<MTLComputeCommandEncoder>)ptEncoder->_pInternal;

    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        MTLSize tGridSize = MTLSizeMake(ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
        MTLSize tThreadsPerGroup = MTLSizeMake(ptDispatch->uThreadPerGroupX, ptDispatch->uThreadPerGroupY, ptDispatch->uThreadPerGroupZ);
        [tComputeEncoder dispatchThreadgroups:tGridSize threadsPerThreadgroup:tThreadsPerGroup];
    }
}

static void
pl_bind_compute_bind_groups(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{   
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    id<MTLComputeCommandEncoder> tComputeEncoder = (id<MTLComputeCommandEncoder>)ptEncoder->_pInternal;

    if(ptDynamicBinding)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
        [tComputeEncoder setBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:uFirst + uCount];
    }

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        [tComputeEncoder useHeap:ptMetalGraphics->sbFrames[i].tDescriptorHeap];
    }

    for(uint32_t i = 0; i < uCount; i++)
    {
        plMetalBindGroup* ptBindGroup = &ptMetalGraphics->sbtBindGroupsHot[atBindGroups[i].uIndex];
        
        for(uint32 j = 0; j < ptBindGroup->uHeapCount; j++)
        {
            [tComputeEncoder useHeap:ptBindGroup->atRequiredHeaps[j]];
        }

        [tComputeEncoder setBuffer:ptBindGroup->tShaderArgumentBuffer
            offset:ptBindGroup->uOffset
            atIndex:uFirst + i];
    }
}

static void
pl_bind_graphics_bind_groups(plRenderEncoder* ptEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, plDynamicBinding* ptDynamicBinding)
{   
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    id<MTLRenderCommandEncoder> tEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;

    if(ptDynamicBinding)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);
        [tEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:uFirst + uCount + 1];
        [tEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:uFirst + uCount + 1];
    }

    for(uint32_t i = 0; i < uCount; i++)
    {
        plMetalBindGroup* ptBindGroup = &ptMetalGraphics->sbtBindGroupsHot[atBindGroups[i].uIndex];

        for(uint32 j = 0; j < ptBindGroup->uHeapCount; j++)
        {
            [tEncoder useHeap:ptBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
        }

        for(uint32_t k = 0; k < ptBindGroup->tLayout.uTextureBindingCount; k++)
        {
            const plTextureHandle tTextureHandle = ptBindGroup->atTextureBindings[k];
            plTexture* ptTexture = pl__get_texture(&ptGraphics->tDevice, tTextureHandle);
            [tEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];  
        }

        [tEncoder setVertexBuffer:ptBindGroup->tShaderArgumentBuffer offset:ptBindGroup->uOffset atIndex:uFirst + i + 1];
        [tEncoder setFragmentBuffer:ptBindGroup->tShaderArgumentBuffer offset:ptBindGroup->uOffset atIndex:uFirst + i + 1];
    }
}

static void
pl_set_viewport(plRenderEncoder* ptEncoder, const plRenderViewport* ptViewport)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    id<MTLRenderCommandEncoder> tEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;

    MTLViewport tViewport = {
        .originX = ptViewport->fX,
        .originY = ptViewport->fY,
        .width   = ptViewport->fWidth,
        .height  = ptViewport->fHeight,
        .znear   = ptViewport->fMinDepth,
        .zfar    = ptViewport->fMaxDepth
    };
    [tEncoder setViewport:tViewport];
}

static void
pl_set_scissor_region(plRenderEncoder* ptEncoder, const plScissor* ptScissor)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    id<MTLRenderCommandEncoder> tEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;

    MTLScissorRect tScissorRect = {
        .x      = (NSUInteger)(ptScissor->iOffsetX),
        .y      = (NSUInteger)(ptScissor->iOffsetY),
        .width  = (NSUInteger)(ptScissor->uWidth),
        .height = (NSUInteger)(ptScissor->uHeight)
    };
    [tEncoder setScissorRect:tScissorRect];
}

static void
pl_bind_vertex_buffer(plRenderEncoder* ptEncoder, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    id<MTLRenderCommandEncoder> tEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;

    [tEncoder setVertexBuffer:ptMetalGraphics->sbtBuffersHot[tHandle.uIndex].tBuffer
        offset:0
        atIndex:0];
}

static void
pl_draw(plRenderEncoder* ptEncoder, uint32_t uCount, const plDraw* atDraws)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    id<MTLRenderCommandEncoder> tEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;

    for(uint32_t i = 0; i < uCount; i++)
    {
        [tEncoder drawPrimitives:MTLPrimitiveTypeTriangle 
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
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    id<MTLRenderCommandEncoder> tEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;

    for(uint32_t i = 0; i < uCount; i++)
    {
        [tEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
            indexCount:atDraws[i].uIndexCount
            indexType:MTLIndexTypeUInt32
            indexBuffer:ptMetalGraphics->sbtBuffersHot[atDraws[i].tIndexBuffer.uIndex].tBuffer
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
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    id<MTLRenderCommandEncoder> tEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;
    plMetalShader* ptMetalShader = &ptMetalGraphics->sbtShadersHot[tHandle.uIndex];

    [tEncoder setStencilReferenceValue:ptMetalShader->ulStencilRef];
    [tEncoder setCullMode:ptMetalShader->tCullMode];
    [tEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [tEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
    [tEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
    [tEncoder setTriangleFillMode:ptMetalShader->tFillMode];
}

static void
pl_bind_compute_shader(plComputeEncoder* ptEncoder, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    id<MTLComputeCommandEncoder> tEncoder = (id<MTLComputeCommandEncoder>)ptEncoder->_pInternal;
    plMetalComputeShader* ptMetalShader = &ptMetalGraphics->sbtComputeShadersHot[tHandle.uIndex];

    [tEncoder setComputePipelineState:ptMetalShader->tPipelineState];
}

static void
pl_draw_stream(plRenderEncoder* ptEncoder, uint32_t uAreaCount, plDrawArea* atAreas)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptEncoder->ptGraphics;
    id<MTLRenderCommandEncoder> tRenderEncoder = (id<MTLRenderCommandEncoder>)ptEncoder->_pInternal;
    id<MTLCommandBuffer> tCmdBuffer = (id<MTLCommandBuffer>)ptEncoder->tCommandBuffer._pInternal;
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;
    id<MTLDevice> tDevice = ptMetalDevice->tDevice;
    plFrameContext* ptFrame = pl__get_frame_resources(ptGraphics);

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        [tRenderEncoder useHeap:ptMetalGraphics->sbFrames[i].tDescriptorHeap stages:MTLRenderStageVertex | MTLRenderStageFragment];
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
        id<MTLDepthStencilState> tCurrentDepthStencilState = nil;

        uint32_t uDynamicSlot = UINT32_MAX;
        while(uCurrentStreamIndex < uTokens)
        {
            const uint32_t uDirtyMask = ptStream->sbtStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                const plShader* ptShader= &ptGraphics->sbtShadersCold[ptStream->sbtStream[uCurrentStreamIndex]];
                plMetalShader* ptMetalShader = &ptMetalGraphics->sbtShadersHot[ptStream->sbtStream[uCurrentStreamIndex]];
                [tRenderEncoder setCullMode:ptMetalShader->tCullMode];
                [tRenderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
                if(tCurrentDepthStencilState != ptMetalShader->tDepthStencilState)
                {
                    [tRenderEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
                }
                tCurrentDepthStencilState = ptMetalShader->tDepthStencilState;
                [tRenderEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
                [tRenderEncoder setTriangleFillMode:ptMetalShader->tFillMode];
                [tRenderEncoder setStencilReferenceValue:ptMetalShader->ulStencilRef];                

                uCurrentStreamIndex++;
                uDynamicSlot = ptShader->tDescription.uBindGroupLayoutCount + 1;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                uDynamicBufferOffset = ptStream->sbtStream[uCurrentStreamIndex];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [tRenderEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureBindingCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptMetalBindGroup->atTextureBindings[k];
                    plTexture* ptTexture = pl__get_texture(&ptGraphics->tDevice, tTextureHandle);
                    [tRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];  
                }

                [tRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:1];
                [tRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:1];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];

                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [tRenderEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureBindingCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptMetalBindGroup->atTextureBindings[k];
                    plTexture* ptTexture = pl__get_texture(&ptGraphics->tDevice, tTextureHandle);
                    [tRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];  
                }

                [tRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:2];
                [tRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:2];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                plMetalBindGroup* ptMetalBindGroup = &ptMetalGraphics->sbtBindGroupsHot[ptStream->sbtStream[uCurrentStreamIndex]];
                
                for(uint32 j = 0; j < ptMetalBindGroup->uHeapCount; j++)
                {
                    [tRenderEncoder useHeap:ptMetalBindGroup->atRequiredHeaps[j] stages:MTLRenderStageVertex | MTLRenderStageFragment];
                }

                for(uint32_t k = 0; k < ptMetalBindGroup->tLayout.uTextureBindingCount; k++)
                {
                    const plTextureHandle tTextureHandle = ptMetalBindGroup->atTextureBindings[k];
                    [tRenderEncoder useResource:ptMetalGraphics->sbtTexturesHot[tTextureHandle.uIndex].tTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment]; 
                }

                [tRenderEncoder setVertexBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:3];
                [tRenderEncoder setFragmentBuffer:ptMetalBindGroup->tShaderArgumentBuffer offset:ptMetalBindGroup->uOffset atIndex:3];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
            {
                
                [tRenderEncoder setVertexBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer offset:0 atIndex:uDynamicSlot];
                [tRenderEncoder setFragmentBuffer:ptFrame->sbtDynamicBuffers[ptStream->sbtStream[uCurrentStreamIndex]].tBuffer offset:0 atIndex:uDynamicSlot];

                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
            {
                [tRenderEncoder setVertexBufferOffset:uDynamicBufferOffset atIndex:uDynamicSlot];
                [tRenderEncoder setFragmentBufferOffset:uDynamicBufferOffset atIndex:uDynamicSlot];
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
pl_flush_device(plDevice* ptDevice)
{
    gptThread->sleep_thread(500);
}

static void
pl_cleanup(plGraphics* ptGraphics)
{
    plGraphicsMetal* ptMetalGraphics = (plGraphicsMetal*)ptGraphics->_pInternalData;

    plDeviceMetal* ptMetalDevice = (plDeviceMetal*)ptGraphics->tDevice._pInternalData;

    for(uint32_t i = 0; i < pl_sb_size(ptMetalGraphics->sbFrames); i++)
    {
        plFrameContext* ptFrame = &ptMetalGraphics->sbFrames[i];
        pl_sb_free(ptFrame->sbtDynamicBuffers);
        pl_sb_free(ptFrame->sbtArgumentBuffers);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptMetalGraphics->sbtRenderPassesHot); i++)
    {
        for(uint32_t j = 0; j < PL_FRAMES_IN_FLIGHT; j++)
        {
            pl_sb_free(ptMetalGraphics->sbtRenderPassesHot[i].atRenderPassDescriptors[j].sbptRenderPassDescriptor);
        }
    }

    pl_sb_free(ptMetalGraphics->sbFrames);
    pl_sb_free(ptMetalGraphics->sbtTexturesHot);
    pl_sb_free(ptMetalGraphics->sbtSamplersHot);
    pl_sb_free(ptMetalGraphics->sbtBindGroupsHot);
    pl_sb_free(ptMetalGraphics->sbtBuffersHot);
    pl_sb_free(ptMetalGraphics->sbtShadersHot);
    pl_sb_free(ptMetalGraphics->sbFrames);
    pl_sb_free(ptMetalGraphics->sbtRenderPassesHot);
    pl_sb_free(ptMetalGraphics->sbtRenderPassLayoutsHot);
    pl_sb_free(ptMetalGraphics->sbtComputeShadersHot);
    pl_sb_free(ptMetalGraphics->sbtSemaphoresHot);
    pl__cleanup_common_graphics(ptGraphics);
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
        for(uint32_t uFrameIndex = 0; uFrameIndex < PL_FRAMES_IN_FLIGHT; uFrameIndex++)
        {
            for(uint32_t j = 0; j < pl_sb_size(ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor); j++)
            {
                [ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[j] release];
                ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor[j] = nil;
            }
            pl_sb_free(ptMetalResource->atRenderPassDescriptors[uFrameIndex].sbptRenderPassDescriptor);
        }
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

        plMetalShader* ptVariantMetalResource = &ptMetalGraphics->sbtShadersHot[iResourceIndex];
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
        pl_sb_push(ptGraphics->sbtShaderFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtComputeShaders); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtComputeShaders[i].uIndex;
        plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[iResourceIndex];

        plMetalComputeShader* ptVariantMetalResource = &ptMetalGraphics->sbtComputeShadersHot[iResourceIndex];
        [ptVariantMetalResource->tPipelineState release];
        ptVariantMetalResource->tPipelineState = nil;
        if(ptVariantMetalResource->library)
        {
            [ptVariantMetalResource->library release];
            ptVariantMetalResource->library = nil;
        }
        pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, iResourceIndex);

    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBindGroups); i++)
    {
        const uint32_t iBindGroupIndex = ptGarbage->sbtBindGroups[i].uIndex;
        plMetalBindGroup* ptMetalResource = &ptMetalGraphics->sbtBindGroupsHot[iBindGroupIndex];
        [ptMetalResource->tShaderArgumentBuffer release];
        ptMetalResource->tShaderArgumentBuffer = nil;
        pl_sb_push(ptGraphics->sbtBindGroupFreeIndices, iBindGroupIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtSamplers); i++)
    {
        const uint32_t iResourceIndex = ptGarbage->sbtSamplers[i].uIndex;
        plMetalSampler* ptMetalSampler = &ptMetalGraphics->sbtSamplersHot[iResourceIndex];
        [ptMetalSampler->tSampler release];
        ptMetalSampler->tSampler = nil;
        pl_sb_push(ptGraphics->sbtSamplerFreeIndices, iResourceIndex);
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint32_t uTextureIndex = ptGarbage->sbtTextures[i].uIndex;
        plMetalTexture* ptMetalTexture = &ptMetalGraphics->sbtTexturesHot[uTextureIndex];
        [ptMetalTexture->tTexture release];
        ptMetalTexture->tTexture = nil;
        pl_sb_push(ptGraphics->sbtTextureFreeIndices, uTextureIndex);
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
        if(ptGarbage->sbtMemory[i].ptAllocator)
        {
            ptGarbage->sbtMemory[i].ptAllocator->free(ptGarbage->sbtMemory[i].ptAllocator->ptInst, &ptGarbage->sbtMemory[i]);
        }
        else
        {
            pl_free_memory(&ptGraphics->tDevice, &ptGarbage->sbtMemory[i]);
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
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;

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
        ptDevice->ptGraphics->szHostMemoryInUse += tBlock.ulSize;
    }
    else if(tMemoryMode == PL_MEMORY_GPU)
    {
        ptHeapDescriptor.storageMode = MTLStorageModePrivate;
        ptDevice->ptGraphics->szLocalMemoryInUse += tBlock.ulSize;
    }

    id<MTLHeap> tNewHeap = [ptMetalDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    tNewHeap.label = [NSString stringWithUTF8String:pcName];
    tBlock.uHandle = (uint64_t)tNewHeap;
    
    [ptHeapDescriptor release];
    return tBlock;
}

static void
pl_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    id<MTLHeap> tHeap = (id<MTLHeap>)ptBlock->uHandle;

    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;

    [tHeap setPurgeableState:MTLPurgeableStateEmpty];
    [tHeap release];
    tHeap = nil;

    if(ptBlock->tMemoryMode == PL_MEMORY_GPU)
    {
        ptDevice->ptGraphics->szLocalMemoryInUse -= ptBlock->ulSize;
    }
    else
    {
        ptDevice->ptGraphics->szHostMemoryInUse -= ptBlock->ulSize;
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
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;

    ptGraphics->sbtBufferGenerations[tHandle.uIndex]++;
    pl_sb_push(ptGraphics->sbtBufferFreeIndices, tHandle.uIndex);

    [ptMetalGraphics->sbtBuffersHot[tHandle.uIndex].tBuffer release];
    ptMetalGraphics->sbtBuffersHot[tHandle.uIndex].tBuffer = nil;

    plBuffer* ptBuffer = &ptGraphics->sbtBuffersCold[tHandle.uIndex];
    if(ptBuffer->tMemoryAllocation.ptAllocator)
        ptBuffer->tMemoryAllocation.ptAllocator->free(ptBuffer->tMemoryAllocation.ptAllocator->ptInst, &ptBuffer->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptBuffer->tMemoryAllocation);
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
    if(ptTexture->tMemoryAllocation.ptAllocator)
        ptTexture->tMemoryAllocation.ptAllocator->free(ptTexture->tMemoryAllocation.ptAllocator->ptInst, &ptTexture->tMemoryAllocation);
    else
        pl_free_memory(ptDevice, &ptTexture->tMemoryAllocation);
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
    for(uint32_t uFrameIndex = 0; uFrameIndex < PL_FRAMES_IN_FLIGHT; uFrameIndex++)
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

    plMetalShader* ptVariantMetalResource = &ptMetalGraphics->sbtShadersHot[tHandle.uIndex];
    [ptVariantMetalResource->tDepthStencilState release];
    [ptVariantMetalResource->tRenderPipelineState release];
    ptVariantMetalResource->tDepthStencilState = nil;
    ptVariantMetalResource->tRenderPipelineState = nil;
    pl_sb_push(ptGraphics->sbtShaderFreeIndices, tHandle.uIndex);

}

static void
pl_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plGraphicsMetal* ptMetalGraphics = ptGraphics->_pInternalData;
    plDeviceMetal* ptMetalDevice = ptDevice->_pInternalData;
    ptGraphics->sbtComputeShaderGenerations[tHandle.uIndex]++;

    plComputeShader* ptResource = &ptGraphics->sbtComputeShadersCold[tHandle.uIndex];
    plMetalComputeShader* ptVariantMetalResource = &ptMetalGraphics->sbtComputeShadersHot[tHandle.uIndex];
    [ptVariantMetalResource->tPipelineState release];
    ptVariantMetalResource->tPipelineState = nil;
    pl_sb_push(ptGraphics->sbtComputeShaderFreeIndices, tHandle.uIndex);
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
        .begin_frame                      = pl_begin_frame,
        .dispatch                         = pl_dispatch,
        .bind_compute_bind_groups         = pl_bind_compute_bind_groups,
        .bind_graphics_bind_groups        = pl_bind_graphics_bind_groups,
        .set_scissor_region               = pl_set_scissor_region,
        .set_viewport                     = pl_set_viewport,
        .bind_vertex_buffer               = pl_bind_vertex_buffer,
        .bind_shader                      = pl_bind_shader,
        .bind_compute_shader              = pl_bind_compute_shader,
        .cleanup                          = pl_cleanup,
        .begin_command_recording          = pl_begin_command_recording,
        .end_command_recording            = pl_end_command_recording,
        .submit_command_buffer            = pl_submit_command_buffer,
        .submit_command_buffer_blocking   = pl_submit_command_buffer_blocking,
        .next_subpass                     = pl_next_subpass,
        .begin_render_pass                = pl_begin_render_pass,
        .end_render_pass                  = pl_end_render_pass,
        .begin_compute_pass               = pl_begin_compute_pass,
        .end_compute_pass                 = pl_end_compute_pass,
        .begin_blit_pass                  = pl_begin_blit_pass,
        .end_blit_pass                    = pl_end_blit_pass,
        .draw_stream                      = pl_draw_stream,
        .draw                             = pl_draw,
        .draw_indexed                     = pl_draw_indexed,
        .present                          = pl_present,
        .copy_buffer_to_texture           = pl_copy_buffer_to_texture,
        .transfer_image_to_buffer         = pl_transfer_image_to_buffer,
        .generate_mipmaps                 = pl_generate_mipmaps,
        .copy_buffer                      = pl_copy_buffer,
        .signal_semaphore                 = pl_signal_semaphore,
        .wait_semaphore                   = pl_wait_semaphore,
        .get_semaphore_value              = pl_get_semaphore_value,
        .reset_draw_stream                = pl_drawstream_reset,
        .add_to_stream                    = pl_drawstream_draw,
        .cleanup_draw_stream              = pl_drawstream_cleanup
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
        .create_sampler                         = pl_create_sampler,
        .create_bind_group                      = pl_create_bind_group,
        .get_temporary_bind_group               = pl_get_temporary_bind_group,
        .update_bind_group                      = pl_update_bind_group,
        .allocate_dynamic_data                  = pl_allocate_dynamic_data,
        .queue_buffer_for_deletion              = pl_queue_buffer_for_deletion,
        .queue_texture_for_deletion             = pl_queue_texture_for_deletion,
        .queue_bind_group_for_deletion          = pl_queue_bind_group_for_deletion,
        .queue_shader_for_deletion              = pl_queue_shader_for_deletion,
        .queue_compute_shader_for_deletion      = pl_queue_compute_shader_for_deletion,
        .queue_render_pass_for_deletion         = pl_queue_render_pass_for_deletion,
        .queue_render_pass_layout_for_deletion  = pl_queue_render_pass_layout_for_deletion,
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
        .get_bind_group                         = pl__get_bind_group,
        .get_shader                             = pl__get_shader,
        .allocate_memory                        = pl_allocate_memory,
        .free_memory                            = pl_free_memory,
        .flush_device                           = pl_flush_device,
        .bind_buffer_to_memory                  = pl_bind_buffer_to_memory,
        .bind_texture_to_memory                 = pl_bind_texture_to_memory,
        .get_sampler                            = pl_get_sampler,
        .get_render_pass                        = pl_get_render_pass,
        .get_render_pass_layout                 = pl_get_render_pass_layout
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    gptFile = ptApiRegistry->first(PL_API_FILE);
    gptThread = ptApiRegistry->first(PL_API_THREADS);
    gptIO = ptApiRegistry->first(PL_API_IO);
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
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{

}

