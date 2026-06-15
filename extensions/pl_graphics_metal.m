
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
    plCommandPool*        ptPool;
    plDevice*             ptDevice;
    id<MTL4CommandBuffer> tCmdBuffer4;
    id<MTLEvent>          tEvent;
    uint64_t              uEventValue;
    plCommandBuffer*      ptNext;
} plCommandBuffer;

typedef struct _plMetalDynamicBuffer
{
    uint32_t                 uHandle;
    plDeviceMemoryAllocation tMemory;
    id<MTLBuffer>            tBuffer;
} plMetalDynamicBuffer;

typedef struct _plMetalBuffer
{
    id<MTLBuffer> tBuffer;
    uint64_t      uHeap;
} plMetalBuffer;

typedef struct _plCommandPool
{
    plDevice*            ptDevice;
    id<MTL4CommandAllocator> tCmdAllocator;
    plCommandBuffer*     ptCommandBufferFreeList;
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
    id<MTLSharedEvent>    tFrameBoundaryEvent;
    uint64_t              uNextValue;
} plFrameContext;

typedef struct _plMetalTexture
{
    id<MTLTexture>        tTexture;
    uint64_t              uHeap;
    MTLTextureDescriptor* ptTextureDescriptor;
    bool                  bOriginalView;
    bool                  bSwapchain;
} plMetalTexture;

typedef struct _plMetalSampler
{
    id<MTLSamplerState> tSampler;
} plMetalSampler;

typedef struct _plTimelineSemaphore
{
    plDevice*            ptDevice;
    id<MTLEvent>         tEvent;
    id<MTLSharedEvent>   tSharedEvent;
    plTimelineSemaphore* ptNext;
} plTimelineSemaphore;

typedef struct _plMetalBindGroup
{
    id<MTLBuffer>         tShaderArgumentBuffer;
    plBindGroupLayoutDesc tLayout;
    plSamplerHandle       atSamplerBindings[PL_MAX_TEXTURES_PER_BIND_GROUP];
    uint32_t              uOffset;
    uint32_t              uSize;
    plTextureHandle       tFirstTexture; // for use with imgui for now (temp)
} plMetalBindGroup;

typedef struct _plMetalShader
{
    id<MTLDepthStencilState>   tDepthStencilState;
    id<MTLRenderPipelineState> tRenderPipelineState;
    MTLCullMode                tCullMode;
    MTLTriangleFillMode        tFillMode;
    MTLDepthClipMode           tDepthClipMode;
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
    
    // metal specifics
    plTempAllocator tTempAllocator;
    CAMetalLayer*   pMetalLayer;
    
    bool bComputeEncoderActive;
    bool bRenderEncoderActive;
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

    // bind group layout generation pool
    plBindGroupLayout* sbtBindGroupLayoutsHot;
    plBindGroupLayout* sbtBindGroupLayoutsCold;
    uint16_t*          sbtBindGroupLayoutFreeIndices;

    // metal specifics
    id<MTLDevice> tDevice;

    id<MTLHeap> atHeaps[64];
    const char* apcHeapNames[64];
    uint64_t*   sbuFreeHeaps;
    
    // memory blocks
    plDeviceMemoryAllocation* sbtMemoryBlocks;

    // compute encoder
    id<MTL4ComputeCommandEncoder> tComputeEncoder;

    // render encoder
    id<MTL4RenderCommandEncoder> tRenderEncoder;
    MTL4RenderPassDescriptor*      ptRenderPassDescriptor;

    id<MTL4ArgumentTable> tArgumentTable;
    id<MTLResidencySet> tResidencySet;
    plStackedBarrier* sbtBarrierStack;

    id<MTL4CommandQueue> tCmdQueue4;
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
    id<CAMetalDrawable> tCurrentDrawable;
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
static MTLPixelFormat         pl__metal_format(plFormat eFormat);
static MTLVertexFormat        pl__metal_vertex_format(plVertexFormat eFormat);
static MTLCullMode            pl__metal_cull(plCullMode tCullMode);
static MTLLoadAction          pl__metal_load_op   (plLoadOp tOp);
static MTLStoreAction         pl__metal_store_op  (plStoreOp tOp);
static MTLDataType            pl__metal_data_type  (plDataType eType);
static MTLRenderStages        pl__metal_stage_flags(plShaderStageFlags tFlags);
static bool                   pl__is_depth_format  (plFormat eFormat);
static bool                   pl__is_stencil_format  (plFormat eFormat);
static MTLBlendFactor         pl__metal_blend_factor(plBlendFactor tFactor);
static MTLBlendOperation      pl__metal_blend_op(plBlendOp tOp);

#define PL__ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_graphics_copy_buffer_to_texture(plCommandBuffer* ptCmdBuffer, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle,
    uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plMetalBuffer*  ptBuffer      = &ptDevice->sbtBuffersHot[tBufferHandle.uIndex];
    plMetalTexture* ptTexture     = &ptDevice->sbtTexturesHot[tTextureHandle.uIndex];
    plTexture*      ptColdTexture = pl_graphics_get_texture(ptDevice, tTextureHandle);

    const uint32_t uFormatStride = pl__format_stride(ptColdTexture->tDesc.eFormat);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        const plBufferImageCopy* ptRegion = &ptRegions[i];

        MTLOrigin tOrigin = {
            .x = ptRegion->iImageOffsetX,
            .y = ptRegion->iImageOffsetY,
            .z = ptRegion->iImageOffsetZ
        };

        MTLSize tSize = {
            .width  = ptRegion->uImageWidth,
            .height = ptRegion->uImageHeight,
            .depth  = ptRegion->uImageDepth
        };

        const NSUInteger uBytesPerRow =
            ptRegion->uBufferRowLength ?
            ptRegion->uBufferRowLength * uFormatStride :
            tSize.width * uFormatStride;

        const NSUInteger uBytesPerImage =
            ptRegion->uBufferImageHeight ?
            uBytesPerRow * ptRegion->uBufferImageHeight :
            uBytesPerRow * tSize.height;

        const uint32_t uLayerCount =
            ptRegion->uLayerCount ? ptRegion->uLayerCount : 1;

        for(uint32_t uLayer = 0; uLayer < uLayerCount; uLayer++)
        {
            const NSUInteger uSourceOffset =
                ptRegion->szBufferOffset + uLayer * uBytesPerImage;

            const NSUInteger uDestinationSlice =
                ptRegion->uBaseArrayLayer + uLayer;

            [ptDevice->tComputeEncoder copyFromBuffer:ptBuffer->tBuffer
                sourceOffset:uSourceOffset
                sourceBytesPerRow:uBytesPerRow
                sourceBytesPerImage:uBytesPerImage
                sourceSize:tSize
                toTexture:ptTexture->tTexture
                destinationSlice:uDestinationSlice
                destinationLevel:ptRegion->uMipLevel
                destinationOrigin:tOrigin];
        }
    }
}

void
pl_graphics_copy_texture(plCommandBuffer* ptCmdBuffer, plTextureHandle tSrcHandle, plTextureHandle tDstHandle, uint32_t uRegionCount, const plImageCopy* ptRegions)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    const plMetalTexture* ptMetalSrcTexture = &ptDevice->sbtTexturesHot[tSrcHandle.uIndex];
    const plMetalTexture* ptMetalDstTexture = &ptDevice->sbtTexturesHot[tDstHandle.uIndex];

    for(uint32_t i = 0; i < uRegionCount; i++)
    {

        MTLOrigin tSrcOrigin;
        tSrcOrigin.x = ptRegions[i].iSourceOffsetX;
        tSrcOrigin.y = ptRegions[i].iSourceOffsetY;
        tSrcOrigin.z = ptRegions[i].iSourceOffsetZ;

        MTLOrigin tDestOrigin;
        tDestOrigin.x = ptRegions[i].iDestinationOffsetX;
        tDestOrigin.y = ptRegions[i].iDestinationOffsetY;
        tDestOrigin.z = ptRegions[i].iDestinationOffsetZ;

        MTLSize tSourceSize;
        tSourceSize.width  = ptRegions[i].uSourceExtentX;
        tSourceSize.height = ptRegions[i].uSourceExtentY;
        tSourceSize.depth  = ptRegions[i].uSourceExtentZ;

        [ptDevice->tComputeEncoder copyFromTexture:ptMetalSrcTexture->tTexture
            sourceSlice:ptRegions[i].uSourceBaseArrayLayer
            sourceLevel:ptRegions[i].uSourceMipLevel
            sourceOrigin:tSrcOrigin
            sourceSize:tSourceSize
            toTexture:ptMetalDstTexture->tTexture
            destinationSlice:ptRegions[i].uDestinationBaseArrayLayer
            destinationLevel:ptRegions[i].uDestinationMipLevel
            destinationOrigin:tDestOrigin];
    }
}

void
pl_graphics_copy_texture_to_buffer(plCommandBuffer* ptCmdBuffer, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle,
    uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    const plTexture* ptTexture = pl_graphics_get_texture(ptDevice, tTextureHandle);
    const plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[tTextureHandle.uIndex];
    const plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[tBufferHandle.uIndex];

    const uint32_t uFormatStride = pl__format_stride(ptTexture->tDesc.eFormat);

    for(uint32_t i = 0; i < uRegionCount; i++)
    {
        const plBufferImageCopy* ptRegion = &ptRegions[i];

        MTLOrigin tOrigin = {
            .x = ptRegion->iImageOffsetX,
            .y = ptRegion->iImageOffsetY,
            .z = ptRegion->iImageOffsetZ
        };

        MTLSize tSize = {
            .width  = ptRegion->uImageWidth,
            .height = ptRegion->uImageHeight,
            .depth  = ptRegion->uImageDepth
        };

        const NSUInteger uBytesPerRow =
            ptRegion->uBufferRowLength ?
            ptRegion->uBufferRowLength * uFormatStride :
            tSize.width * uFormatStride;

        const NSUInteger uBytesPerImage =
            ptRegion->uBufferImageHeight ?
            uBytesPerRow * ptRegion->uBufferImageHeight :
            uBytesPerRow * tSize.height;

        const uint32_t uLayerCount =
            ptRegion->uLayerCount ? ptRegion->uLayerCount : 1;

        for(uint32_t uLayer = 0; uLayer < uLayerCount; uLayer++)
        {
            const NSUInteger uDestinationOffset =
                ptRegion->szBufferOffset + uLayer * uBytesPerImage;

            const NSUInteger uSourceSlice =
                ptRegion->uBaseArrayLayer + uLayer;

            [ptDevice->tComputeEncoder copyFromTexture:ptMetalTexture->tTexture
                sourceSlice:uSourceSlice
                sourceLevel:ptRegion->uMipLevel
                sourceOrigin:tOrigin
                sourceSize:tSize
                toBuffer:ptMetalBuffer->tBuffer
                destinationOffset:uDestinationOffset
                destinationBytesPerRow:uBytesPerRow
                destinationBytesPerImage:uBytesPerImage];
        }
    }
}

void
pl_graphics_copy_buffer(plCommandBuffer* ptCmdBuffer, plBufferHandle tSource, plBufferHandle tDestination, uint64_t uSourceOffset, uint64_t uDestinationOffset, size_t szSize)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    [ptDevice->tComputeEncoder copyFromBuffer:ptDevice->sbtBuffersHot[tSource.uIndex].tBuffer sourceOffset:uSourceOffset toBuffer:ptDevice->sbtBuffersHot[tDestination.uIndex].tBuffer destinationOffset:uDestinationOffset size:szSize];
}

plTimelineSemaphore*
pl_graphics_create_semaphore(plDevice* ptDevice, bool bHostVisible)
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

void
pl_graphics_cleanup_semaphore(plTimelineSemaphore* ptSemaphore)
{
    pl__return_semaphore(ptSemaphore->ptDevice, ptSemaphore);
}

void
pl_graphics_signal_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
    PL_ASSERT(ptSemaphore->tSharedEvent != nil);
    if(ptSemaphore->tSharedEvent)
    {
        ptSemaphore->tSharedEvent.signaledValue = ulValue;
    }
}

void
pl_graphics_wait_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
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

uint64_t
pl_graphics_get_semaphore_value(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore)
{
    PL_ASSERT(ptSemaphore->tSharedEvent != nil);

    if(ptSemaphore->tSharedEvent)
    {
        return ptSemaphore->tSharedEvent.signaledValue;
    }
    return 0;
}

plBufferHandle
pl_graphics_create_buffer(plDevice* ptDevice, const plBufferDesc* ptDesc, plBuffer** ptBufferOut)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);
    plBuffer* ptBuffer = pl_graphics_get_buffer(ptDevice, tHandle);

    ptBuffer->tDesc = *ptDesc;

    if(ptDesc->pcDebugName == NULL)
        ptBuffer->tDesc.pcDebugName = "unnamed buffer";

    MTLResourceOptions tStorageMode = MTLResourceStorageModePrivate;
    if(ptDesc->eUsage & PL_BUFFER_USAGE_TRANSFER)
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

void
pl_graphics_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plBuffer* ptBuffer = pl_graphics_get_buffer(ptDevice, tHandle);
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];

    MTLResourceOptions tStorageMode = MTLResourceStorageModeShared;
    if(ptAllocation->tMemoryFlags & PL_MEMORY_FLAGS_DEVICE_LOCAL)
    {
        tStorageMode = MTLResourceStorageModePrivate;
    }

    ptMetalBuffer->tBuffer = [ptDevice->atHeaps[ptAllocation->uHandle] newBufferWithLength:ptAllocation->ulSize options:tStorageMode offset:ptAllocation->ulOffset];
    ptMetalBuffer->tBuffer.label = [NSString stringWithUTF8String:ptBuffer->tDesc.pcDebugName];

    if(!(ptAllocation->tMemoryFlags & PL_MEMORY_FLAGS_DEVICE_LOCAL))
    {
        memset(ptMetalBuffer->tBuffer.contents, 0, ptAllocation->ulSize);
        ptBuffer->tMemoryAllocation.pHostMapped = ptMetalBuffer->tBuffer.contents;
    }
    ptMetalBuffer->uHeap = ptAllocation->uHandle;
    [ptDevice->tResidencySet addAllocation:ptMetalBuffer->tBuffer];
    [ptDevice->tResidencySet commit];
    [ptDevice->tResidencySet requestResidency];
}

void
pl_graphics_generate_mipmaps(plCommandBuffer* ptCmdBuffer, plTextureHandle tTexture)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    plTexture* ptTexture = pl_graphics_get_texture(ptDevice, tTexture);
    if(ptTexture->tDesc.uMips < 2)
        return;

    [ptDevice->tComputeEncoder generateMipmapsForTexture:ptDevice->sbtTexturesHot[tTexture.uIndex].tTexture];
}

plTextureHandle
pl_graphics_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, plTexture** ptTextureOut)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    plTexture* ptTexture = pl_graphics_get_texture(ptDevice, tHandle);

    plTextureDesc tDesc = *ptDesc;

    if(tDesc.pcDebugName == NULL)
        tDesc.pcDebugName = "unnamed texture";

    if(tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1;

    for(uint32_t uMipLevel = 1; uMipLevel < tDesc.uMips; uMipLevel++)
    {
        int iCurrentWidth = (int)tDesc.tDimensions.x / ((1 << (int)uMipLevel));
        int iCurrentHeight = (int)tDesc.tDimensions.y / ((1 << (int)uMipLevel));

        if(iCurrentHeight < 4 || iCurrentWidth < 4)
        {
            tDesc.uMips = uMipLevel;
            break;
        }
    }

    ptTexture->tDesc = tDesc,
    ptTexture->tView = (plTextureViewDesc){
        .eFormat = tDesc.eFormat,
        .uBaseMip = 0,
        .uMips = tDesc.uMips,
        .uBaseLayer = 0,
        .uLayerCount = tDesc.uLayers,
        .tTexture = tHandle
    };

    MTLTextureDescriptor* ptTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    ptTextureDescriptor.pixelFormat = pl__metal_format(tDesc.eFormat);
    ptTextureDescriptor.width = tDesc.tDimensions.x;
    ptTextureDescriptor.height = tDesc.tDimensions.y;
    ptTextureDescriptor.mipmapLevelCount = tDesc.uMips;
    ptTextureDescriptor.arrayLength = tDesc.eType == PL_TEXTURE_TYPE_2D_ARRAY ? tDesc.uLayers : 1;
    ptTextureDescriptor.depth = tDesc.tDimensions.z;
    ptTextureDescriptor.sampleCount = ptDesc->eSampleCount == 0 ? 1 : ptDesc->eSampleCount;

    if(tDesc.eUsage & PL_TEXTURE_USAGE_SAMPLED)
        ptTextureDescriptor.usage |= MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    if(tDesc.eUsage & PL_TEXTURE_USAGE_INPUT_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageShaderRead;    
    if(tDesc.eUsage & PL_TEXTURE_USAGE_COLOR_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;
    if(tDesc.eUsage & PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT)
        ptTextureDescriptor.usage |= MTLTextureUsageRenderTarget;
    if(tDesc.eUsage & PL_TEXTURE_USAGE_STORAGE)
        ptTextureDescriptor.usage |= MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;

    if(tDesc.eType == PL_TEXTURE_TYPE_2D)
        ptTextureDescriptor.textureType = MTLTextureType2D;
    else if(tDesc.eType == PL_TEXTURE_TYPE_CUBE)
        ptTextureDescriptor.textureType = MTLTextureTypeCube;
    else if(tDesc.eType == PL_TEXTURE_TYPE_2D_ARRAY)
    {
        ptTextureDescriptor.textureType = MTLTextureType2DArray;
    }
    else
    {
        PL_ASSERT(false && "unsupported texture type");
    }

    if(ptDesc->eSampleCount > 1)
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

plSamplerHandle
pl_graphics_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);
    plSampler* ptSampler = pl_graphics_get_sampler(ptDevice, tHandle);
    ptSampler->tDesc = *ptDesc;

    MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
    samplerDesc.minFilter = pl__metal_filter(ptDesc->eMinFilter);
    samplerDesc.magFilter = pl__metal_filter(ptDesc->eMagFilter);
    samplerDesc.mipFilter = ptDesc->eMipmapMode == PL_MIPMAP_MODE_LINEAR ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
    samplerDesc.normalizedCoordinates = ptDesc->bUnnormalizedCoordinates ? NO : YES;
    samplerDesc.supportArgumentBuffers = YES;
    samplerDesc.sAddressMode = pl__metal_wrap(ptDesc->eUAddressMode);
    samplerDesc.tAddressMode = pl__metal_wrap(ptDesc->eVAddressMode);
    samplerDesc.rAddressMode = pl__metal_wrap(ptDesc->eWAddressMode);
    samplerDesc.compareFunction = pl__metal_compare(ptDesc->eCompare);
    samplerDesc.lodMinClamp = ptDesc->fMinMip;
    samplerDesc.lodMaxClamp = ptDesc->fMaxMip;
    samplerDesc.lodBias = ptDesc->fMipBias; // not available until MacOS 26
    samplerDesc.label = [NSString stringWithUTF8String:ptDesc->pcDebugName];

    switch(ptDesc->eBorderColor)
    {
        case PL_BORDER_COLOR_INT_TRANSPARENT_BLACK:
        case PL_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK: samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack; break;

        case PL_BORDER_COLOR_INT_OPAQUE_BLACK:
        case PL_BORDER_COLOR_FLOAT_OPAQUE_BLACK: samplerDesc.borderColor = MTLSamplerBorderColorOpaqueBlack; break;

        case PL_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
        case PL_BORDER_COLOR_INT_OPAQUE_WHITE:   samplerDesc.borderColor = MTLSamplerBorderColorOpaqueWhite; break;
        
        default:
            samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack;
    }

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

plBindGroupLayoutHandle
pl_graphics_create_bind_group_layout(plDevice* ptDevice, const plBindGroupLayoutDesc* ptDesc)
{
    plBindGroupLayoutHandle tHandle = pl__get_new_bind_group_layout_handle(ptDevice);
    plBindGroupLayout* ptLayout = &ptDevice->sbtBindGroupLayoutsCold[tHandle.uIndex];
    
    ptLayout->tDesc = *ptDesc;
    ptLayout->_uBufferBindingCount = 0;
    ptLayout->_uTextureBindingCount = 0;
    ptLayout->_uSamplerBindingCount = 0;
    
    // count bindings
    for(uint32_t i = 0; i < PL_MAX_TEXTURES_PER_BIND_GROUP; i++)
    {
        if(ptDesc->atTextureBindings[i].eStages == PL_SHADER_STAGE_NONE)
            break;
        ptLayout->_uTextureBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_BUFFERS_PER_BIND_GROUP; i++)
    {
        if(ptDesc->atBufferBindings[i].eStages == PL_SHADER_STAGE_NONE)
            break;
        ptLayout->_uBufferBindingCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_SAMPLERS_PER_BIND_GROUP; i++)
    {
        if(ptDesc->atSamplerBindings[i].eStages == PL_SHADER_STAGE_NONE)
            break;
        ptLayout->_uSamplerBindingCount++;
    }

    uint32_t uDescriptorCount = ptLayout->_uTextureBindingCount + ptLayout->_uBufferBindingCount + ptLayout->_uSamplerBindingCount;

    for(uint32_t i = 0; i < ptLayout->_uTextureBindingCount; i++)
    {
        uint32_t uCurrentDescriptorCount = ptDesc->atTextureBindings[i].uDescriptorCount;
        if(uCurrentDescriptorCount == 0)
            uCurrentDescriptorCount = 1;
        if(uCurrentDescriptorCount > 1)
            uDescriptorCount += ptDesc->atTextureBindings[i].uDescriptorCount - 1;
    }

    ptLayout->_uDescriptorCount = uDescriptorCount;

    return tHandle;
}

plBindGroupHandle
pl_graphics_create_bind_group(plDevice* ptDevice, const plBindGroupDesc* ptDesc)
{
    
    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);
    plBindGroup* ptBindGroup = pl_graphics_get_bind_group(ptDevice, tHandle);
    ptBindGroup->tDesc = *ptDesc;
    plBindGroupLayout* ptBindGroupLayout = pl_graphics_get_bind_group_layout(ptDevice, ptDesc->tLayout);

    plBindGroupLayoutDesc* ptLayout = &ptBindGroupLayout->tDesc;
    ptBindGroup->_uDescriptorCount = ptBindGroupLayout->_uDescriptorCount;

    NSUInteger argumentBufferLength = sizeof(uint64_t) * ptBindGroupLayout->_uDescriptorCount;

    plMetalBindGroup tMetalBindGroup = {
        .tLayout = *ptLayout,
    };

    tMetalBindGroup.tShaderArgumentBuffer = ptDesc->ptPool->tArgumentBuffer.tBuffer;
    tMetalBindGroup.uOffset = ptDesc->ptPool->szCurrentArgumentOffset;
    tMetalBindGroup.uSize = argumentBufferLength;

    PL_ASSERT(ptDesc->ptPool->szCurrentArgumentOffset + argumentBufferLength <= ptDesc->ptPool->szHeapSize);
    ptDesc->ptPool->szCurrentArgumentOffset += argumentBufferLength;
    [tMetalBindGroup.tShaderArgumentBuffer retain];
    if(ptDesc->pcDebugName)
        tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:ptDesc->pcDebugName];
    else
        tMetalBindGroup.tShaderArgumentBuffer.label = [NSString stringWithUTF8String:"unnamed bind group"];
    ptDevice->sbtBindGroupsHot[tHandle.uIndex] = tMetalBindGroup;
    return tHandle;
}

void
pl_graphics_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{

    uint32_t uBufferCount = 0;
    uint32_t uTextureCount = 0;
    uint32_t uSamplerCount = 0;

    for (uint32_t i = 0; i < PL_MAX_BUFFERS_PER_BIND_GROUP; i++)
    {

        if(!pl_graphics_is_buffer_valid(ptDevice, ptData->atBufferBindings[i].tBuffer))
            break;
        uBufferCount++;
    }

    for (uint32_t i = 0; i < PL_MAX_TEXTURES_PER_BIND_GROUP; i++)
    {

        if(!pl_graphics_is_texture_valid(ptDevice, ptData->atTextureBindings[i].tTexture))
            break;
        uTextureCount++;
    }

    for (uint32_t i = 0; i < PL_MAX_SAMPLERS_PER_BIND_GROUP; i++)
    {

        if(!pl_graphics_is_sampler_valid(ptDevice, ptData->atSamplerBindings[i].tSampler))
            break;
        uSamplerCount++;
    }

    plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    plBindGroup* ptBindGroup = pl_graphics_get_bind_group(ptDevice, tHandle);
    const char* pcDescriptorStart = ptMetalBindGroup->tShaderArgumentBuffer.contents;

    uint64_t* pulDescriptorStart = (uint64_t*)&pcDescriptorStart[ptMetalBindGroup->uOffset];

    for(uint32_t i = 0; i < uBufferCount; i++)
    {
        const plBindGroupUpdateBufferData* ptUpdate = &ptData->atBufferBindings[i];
        plMetalBuffer* ptMetalBuffer = &ptDevice->sbtBuffersHot[ptUpdate->tBuffer.uIndex];
        uint64_t* ppfDestination = &pulDescriptorStart[ptUpdate->uSlot];
        *ppfDestination = ptMetalBuffer->tBuffer.gpuAddress;
    }

    for(uint32_t i = 0; i < uTextureCount; i++)
    {
        const plBindGroupUpdateTextureData* ptUpdate = &ptData->atTextureBindings[i];
        plTexture* ptTexture = pl_graphics_get_texture(ptDevice, ptUpdate->tTexture);
        if(i == 0)
            ptMetalBindGroup->tFirstTexture = ptUpdate->tTexture;
        plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[ptUpdate->tTexture.uIndex];
        MTLResourceID* pptDestination = (MTLResourceID*)&pulDescriptorStart[ptUpdate->uSlot + ptUpdate->uIndex];
        *pptDestination = ptMetalTexture->tTexture.gpuResourceID;
    }

    for(uint32_t i = 0; i < uSamplerCount; i++)
    {
        const plBindGroupUpdateSamplerData* ptUpdate = &ptData->atSamplerBindings[i];
        plMetalSampler* ptMetalSampler = &ptDevice->sbtSamplersHot[ptUpdate->tSampler.uIndex];
        MTLResourceID* pptDestination = (MTLResourceID*)&pulDescriptorStart[ptUpdate->uSlot];
        *pptDestination = ptMetalSampler->tSampler.gpuResourceID;
        ptMetalBindGroup->atSamplerBindings[i] = ptUpdate->tSampler;
    }
}

plBindGroupPool*
pl_graphics_create_bind_group_pool(plDevice* ptDevice, const plBindGroupPoolDesc* ptDesc)
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

    ptPool->szHeapSize = szMaxSets * sizeof(uint64_t);

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

    memset(tArgumentBuffer.tBuffer.contents, 0, ptPool->szHeapSize);

    ptPool->tArgumentBuffer = tArgumentBuffer;
    [ptDevice->tResidencySet addAllocation:ptPool->tDescriptorHeap];
    [ptDevice->tResidencySet commit];
    [ptDevice->tResidencySet requestResidency];
    return ptPool;
}

void
pl_graphics_reset_bind_group_pool(plBindGroupPool* ptPool)
{
    ptPool->szCurrentArgumentOffset = 0;
}

void
pl_graphics_cleanup_bind_group_pool(plBindGroupPool* ptPool)
{
    PL_FREE(ptPool);
}

void
pl_graphics_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plTexture* ptTexture = pl_graphics_get_texture(ptDevice, tHandle);
    ptTexture->tMemoryAllocation = *ptAllocation;
    plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];

    MTLStorageMode tStorageMode = MTLStorageModeShared;
    if(ptAllocation->tMemoryFlags & PL_MEMORY_FLAGS_DEVICE_LOCAL)
    {
        tStorageMode = MTLStorageModePrivate;
    }
    ptMetalTexture->ptTextureDescriptor.storageMode = tStorageMode;

    ptMetalTexture->tTexture = [ptDevice->atHeaps[ptAllocation->uHandle] newTextureWithDescriptor:ptMetalTexture->ptTextureDescriptor offset:ptAllocation->ulOffset];
    ptMetalTexture->uHeap = ptAllocation->uHandle;
    ptMetalTexture->tTexture.label = [NSString stringWithUTF8String:ptTexture->tDesc.pcDebugName];
    [ptMetalTexture->ptTextureDescriptor release];
    ptMetalTexture->ptTextureDescriptor = nil;
    [ptDevice->tResidencySet addAllocation:ptMetalTexture->tTexture];
    [ptDevice->tResidencySet commit];
    [ptDevice->tResidencySet requestResidency];
}

plTextureHandle
pl_graphics_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    plTexture* ptNewTexture = pl_graphics_get_texture(ptDevice, tHandle);
    plTexture* ptOriginalTexture = pl_graphics_get_texture(ptDevice, ptViewDesc->tTexture);

    ptNewTexture->tDesc = ptOriginalTexture->tDesc;
    ptNewTexture->tView = *ptViewDesc;
    ptNewTexture->tDesc.uMips = ptViewDesc->uMips;
    ptNewTexture->tDesc.uLayers = ptViewDesc->uLayerCount;
    ptNewTexture->tDesc.eType = ptViewDesc->eType;
    ptNewTexture->tView.uBaseMip = 0;
    ptNewTexture->tView.uBaseLayer = 0;

    plMetalTexture* ptOldMetalTexture = &ptDevice->sbtTexturesHot[ptViewDesc->tTexture.uIndex];

    plMetalTexture* ptNewMetalTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];
    ptNewMetalTexture->bOriginalView = false;

    MTLTextureType tTextureType = MTLTextureType2D;

    if(ptNewTexture->tDesc.eType == PL_TEXTURE_TYPE_2D)
        tTextureType = MTLTextureType2D;
    else if(ptNewTexture->tDesc.eType == PL_TEXTURE_TYPE_CUBE)
        tTextureType = MTLTextureTypeCube;
    else if(ptNewTexture->tDesc.eType == PL_TEXTURE_TYPE_2D_ARRAY)
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

    ptNewMetalTexture->tTexture = [ptOldMetalTexture->tTexture newTextureViewWithPixelFormat:pl__metal_format(ptViewDesc->eFormat) 
            textureType:tTextureType
            levels:tLevelRange
            slices:tSliceRange];

    if(ptNewTexture->tView.pcDebugName == NULL)
        ptNewTexture->tView.pcDebugName = "unnamed texture";
    ptNewMetalTexture->tTexture.label = [NSString stringWithUTF8String:ptNewTexture->tView.pcDebugName];
    ptNewMetalTexture->uHeap = ptOldMetalTexture->uHeap;
    return tHandle;
}

void
pl_graphics_reset_dynamic_data_blocks(plDevice* ptDevice)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    ptFrame->uCurrentBufferIndex = 0;
}

plDynamicDataBlock
pl_graphics_allocate_dynamic_data_block(plDevice* ptDevice)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);


    plMetalDynamicBuffer* ptDynamicBuffer = NULL;
    const uint32_t uDynamicBufferCount = pl_sb_size(ptFrame->sbtDynamicBuffers);

    // first call this frame
    // if(ptFrame->uCurrentBufferIndex != 0)
    {
        if(uDynamicBufferCount == 0 || uDynamicBufferCount <= ptFrame->uCurrentBufferIndex)
        {
            pl_sb_add(ptFrame->sbtDynamicBuffers);
            ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];
            static char atNameBuffer[64] = {0};
            pl_sprintf(atNameBuffer, "D-BUF-F%d-%d", (int)gptGraphics->uCurrentFrameIndex, (int)ptFrame->uCurrentBufferIndex);

            ptDynamicBuffer->tMemory = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, 0, ptDevice->tInit.szDynamicBufferBlockSize, 0, atNameBuffer);
            ptDynamicBuffer->tBuffer = [ptDevice->atHeaps[ptDynamicBuffer->tMemory.uHandle] newBufferWithLength:ptDevice->tInit.szDynamicBufferBlockSize options:MTLResourceStorageModeShared offset:ptDynamicBuffer->tMemory.ulOffset];
            [ptDevice->tResidencySet addAllocation:ptDynamicBuffer->tBuffer];
            [ptDevice->tResidencySet commit];
            [ptDevice->tResidencySet requestResidency];
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
    tBlock._uBumpAmount = pl_min(tBlock._uAlignment, tBlock._uBumpAmount);
    if(uDynamicBufferCount > 0)
        ptFrame->uCurrentBufferIndex++;
    return tBlock;
}

plComputeShaderHandle
pl_graphics_create_compute_shader(plDevice* ptDevice, const plComputeShaderDesc* ptDescription)
{
    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);
    plComputeShader* ptShader = pl_graphics_get_compute_shader(ptDevice, tHandle);
    ptShader->tDesc = *ptDescription;
    ptShader->tDesc._uBindGroupLayoutCount = 0;
    ptShader->tDesc._uConstantCount = 0;

    if(ptShader->tDesc.pcDebugName == NULL)
        ptShader->tDesc.pcDebugName = "unnamed shader";

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
        if(ptConstant->eType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        uTotalConstantSize += pl_graphics_get_data_type_size(ptConstant->eType);
        ptShader->tDesc._uConstantCount++;
    }

    for (uint32_t i = 0; i < 3; i++)
    {
        if(ptShader->tDesc.atBindGroupLayouts[i].atTextureBindings[0].eStages == PL_SHADER_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atBufferBindings[0].eStages == PL_SHADER_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atSamplerBindings[0].eStages == PL_SHADER_STAGE_NONE)
        {
            break;
        }
        ptShader->tDesc._uBindGroupLayoutCount++;
    }

    MTLFunctionConstantValues* ptConstantValues = [MTLFunctionConstantValues new];

    const char* pcConstantData = ptDescription->pTempConstantData;
    uint32_t uConstantOffset = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atConstants[i];
        const uint32_t uConstantIndex = ptConstant->uID == 0 ? i : ptConstant->uID;
        const uint32_t uAutoConstantOffset = ptConstant->uOffset == 0 ? uConstantOffset : ptConstant->uOffset;
        [ptConstantValues setConstantValue:&pcConstantData[uAutoConstantOffset] type:pl__metal_data_type(ptConstant->eType) atIndex:uConstantIndex];
        uConstantOffset += (uint32_t)pl_graphics_get_data_type_size(ptConstant->eType);
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

plShaderHandle
pl_graphics_create_shader(plDevice* ptDevice, const plShaderDesc* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);
    plShader* ptShader = pl_graphics_get_shader(ptDevice, tHandle);
    ptShader->tDesc = *ptDescription;
    ptShader->tDesc._uBindGroupLayoutCount = 0;
    ptShader->tDesc._uVertexConstantCount = 0;
    ptShader->tDesc._uFragmentConstantCount = 0;

    if(ptShader->tDesc.pcDebugName == NULL)
        ptShader->tDesc.pcDebugName = "unnamed shader";

    plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    if(ptShader->tDesc.tFragmentShader.pcEntryFunc == NULL)
        ptShader->tDesc.tFragmentShader.pcEntryFunc = "fragment_main";

    if(ptShader->tDesc.tVertexShader.pcEntryFunc == NULL)
        ptShader->tDesc.tVertexShader.pcEntryFunc = "vertex_main";

    NSString* vertexEntry = [NSString stringWithUTF8String:"vertex_main"];
    NSString* fragmentEntry = [NSString stringWithUTF8String:"fragment_main"];

    // vertex layout
    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];

    uint32_t uVertexBufferCount = 0;
    uint32_t uCurrentAttributeCount = 0;
    bool abExplicitAttributePosition[2] = {0};
    bool abExplicitOffset[2] = {0};
    bool abExplicitStride[2] = {0};
    size_t auCalculatedStrides[2] = {0};

    for(uint32_t uVtxBufferIdx = 0; uVtxBufferIdx < 2; uVtxBufferIdx++)
    {
        if(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[0].eFormat == PL_VERTEX_FORMAT_UNKNOWN)
            break;

        if(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].uByteStride != 0)
            abExplicitStride[uVtxBufferIdx] = true;

        uVertexBufferCount++;
        uint32_t uByteStride = 0;
        for (uint32_t i = 0; i < PL_MAX_VERTEX_ATTRIBUTES; i++)
        {
            if (ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].eFormat == PL_VERTEX_FORMAT_UNKNOWN)
                break;

            auCalculatedStrides[uVtxBufferIdx] += pl__get_vertex_attribute_size(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].eFormat);
            if(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uLocation != 0)
            {
                abExplicitAttributePosition[uVtxBufferIdx] = true;
            }

            if(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uByteOffset != 0)
            {
                abExplicitOffset[uVtxBufferIdx] = true;
            }
        }
    }

    for(uint32_t uVtxBufferIdx = 0; uVtxBufferIdx < uVertexBufferCount; uVtxBufferIdx++)
    {
        size_t uOffset = 0;
        for (uint32_t i = 0; i < PL_MAX_VERTEX_ATTRIBUTES; i++)
        {
            if(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].eFormat == PL_VERTEX_FORMAT_UNKNOWN)
                break;
            const uint32_t uLocation = abExplicitAttributePosition[uVtxBufferIdx] ? ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uLocation : uCurrentAttributeCount;
            vertexDescriptor.attributes[uLocation].bufferIndex = 4 + uVtxBufferIdx;
            vertexDescriptor.attributes[uLocation].offset = abExplicitOffset[uVtxBufferIdx] ? ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uByteOffset : (uint32_t)uOffset;
            vertexDescriptor.attributes[uLocation].format = pl__metal_vertex_format(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].eFormat);
            uOffset += pl__get_vertex_attribute_size(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].eFormat);
            uCurrentAttributeCount++;

        }

        vertexDescriptor.layouts[4 + uVtxBufferIdx].stepRate = 1;
        vertexDescriptor.layouts[4 + uVtxBufferIdx].stepFunction = MTLVertexStepFunctionPerVertex;
        vertexDescriptor.layouts[4 + uVtxBufferIdx].stride = abExplicitStride[uVtxBufferIdx] ? ptDescription->atVertexBufferLayouts[uVtxBufferIdx].uByteStride : (uint32_t)auCalculatedStrides[uVtxBufferIdx];
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

    if(ptShader->tDesc.tFragmentShader.puCode)
    {
        NSString* fragmentSource = [NSString stringWithUTF8String:(const char*)ptShader->tDesc.tFragmentShader.puCode];
        ptMetalShader->tFragmentLibrary = [ptDevice->tDevice  newLibraryWithSource:fragmentSource options:ptCompileOptions error:&error];
        if (ptMetalShader->tFragmentLibrary == nil)
        {
            NSLog(@"Error: failed to create Metal fragment library: %@", error);
        }
    }

    pl_temp_allocator_reset(&gptGraphics->tTempAllocator);

    size_t uTotalVertexConstantSize = 0;
    for(uint32_t i = 0; i < PL_MAX_SHADER_SPECIALIZATION_CONSTANTS; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atVertexConstants[i];
        if(ptConstant->eType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        uTotalVertexConstantSize += pl_graphics_get_data_type_size(ptConstant->eType);
        ptShader->tDesc._uVertexConstantCount++;
    }

    size_t uTotalFragmentConstantSize = 0;
    for(uint32_t i = 0; i < PL_MAX_SHADER_SPECIALIZATION_CONSTANTS; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atFragmentConstants[i];
        if(ptConstant->eType == PL_DATA_TYPE_UNSPECIFIED)
            break;
        uTotalFragmentConstantSize += pl_graphics_get_data_type_size(ptConstant->eType);
        ptShader->tDesc._uFragmentConstantCount++;
    }

    for (uint32_t i = 0; i < 3; i++)
    {
        if(ptShader->tDesc.atBindGroupLayouts[i].atTextureBindings[0].eStages == PL_SHADER_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atBufferBindings[0].eStages == PL_SHADER_STAGE_NONE &&
            ptShader->tDesc.atBindGroupLayouts[i].atSamplerBindings[0].eStages == PL_SHADER_STAGE_NONE)
        {
            break;
        }
        ptShader->tDesc._uBindGroupLayoutCount++;
    }

    MTLFunctionConstantValues* ptVertexConstantValues = [MTLFunctionConstantValues new];
    MTLFunctionConstantValues* ptFragmentConstantValues = [MTLFunctionConstantValues new];

    const char* pcVertexConstantData = ptDescription->pVertexTempConstantData;
    uint32_t uConstantOffset = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uVertexConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atVertexConstants[i];
        const uint32_t uConstantIndex = ptConstant->uID == 0 ? i : ptConstant->uID;
        const uint32_t uAutoConstantOffset = ptConstant->uOffset == 0 ? uConstantOffset : ptConstant->uOffset;
        [ptVertexConstantValues setConstantValue:&pcVertexConstantData[uAutoConstantOffset] type:pl__metal_data_type(ptConstant->eType) atIndex:uConstantIndex];
        uConstantOffset += (uint32_t)pl_graphics_get_data_type_size(ptConstant->eType);
    }

    const char* pcFragmentConstantData = ptDescription->pFragmentTempConstantData;
    uConstantOffset = 0;
    for(uint32_t i = 0; i < ptShader->tDesc._uFragmentConstantCount; i++)
    {
        const plSpecializationConstant* ptConstant = &ptShader->tDesc.atFragmentConstants[i];
        const uint32_t uConstantIndex = ptConstant->uID == 0 ? i : ptConstant->uID;
        const uint32_t uAutoConstantOffset = ptConstant->uOffset == 0 ? uConstantOffset : ptConstant->uOffset;
        [ptFragmentConstantValues setConstantValue:&pcFragmentConstantData[uAutoConstantOffset] type:pl__metal_data_type(ptConstant->eType) atIndex:uConstantIndex];
        uConstantOffset += (uint32_t)pl_graphics_get_data_type_size(ptConstant->eType);
    }

    id<MTLFunction> vertexFunction = [ptMetalShader->tVertexLibrary newFunctionWithName:vertexEntry constantValues:ptVertexConstantValues error:&error];
    id<MTLFunction> fragmentFunction = nil;

    if (vertexFunction == nil)
    {
        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
    }

    if(ptShader->tDesc.tFragmentShader.puCode)
    {
        fragmentFunction = [ptMetalShader->tFragmentLibrary newFunctionWithName:fragmentEntry constantValues:ptFragmentConstantValues error:&error];
        if (fragmentFunction == nil)
        {
            NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
        }
    }

    MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
    depthDescriptor.depthCompareFunction = pl__metal_compare((plCompareMode)ptDescription->tGraphicsState.eDepthMode);
    depthDescriptor.depthWriteEnabled = ptDescription->tGraphicsState.bDepthWriteEnabled ? YES : NO;

    if(ptDescription->tGraphicsState.bStencilTestEnabled)
    {
        MTLStencilDescriptor* ptStencilDescriptor = [MTLStencilDescriptor new];
        ptStencilDescriptor.readMask = (uint32_t)ptDescription->tGraphicsState.uStencilMask;
        ptStencilDescriptor.writeMask = (uint32_t)ptDescription->tGraphicsState.uStencilMask;
        ptStencilDescriptor.stencilCompareFunction    = pl__metal_compare((plCompareMode)ptDescription->tGraphicsState.eStencilMode);
        ptStencilDescriptor.stencilFailureOperation   = pl__metal_stencil_op((plStencilOp)ptDescription->tGraphicsState.eStencilOpFail),
        ptStencilDescriptor.depthFailureOperation     = pl__metal_stencil_op((plStencilOp)ptDescription->tGraphicsState.eStencilOpDepthFail),
        ptStencilDescriptor.depthStencilPassOperation = pl__metal_stencil_op((plStencilOp)ptDescription->tGraphicsState.eStencilOpPass),
        depthDescriptor.backFaceStencil = ptStencilDescriptor;
        depthDescriptor.frontFaceStencil = ptStencilDescriptor;
    }
    ptMetalShader->ulStencilRef = ptDescription->tGraphicsState.uStencilRef;

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.rasterSampleCount = ptDescription->eMSAASampleCount == 0 ? 1 : ptDescription->eMSAASampleCount;

    if(ptDescription->tRenderAttachmentInfo.eDepthFormat != 0)
    {
        pipelineDescriptor.depthAttachmentPixelFormat = pl__metal_format(ptDescription->tRenderAttachmentInfo.eDepthFormat);
    }

    if(ptDescription->tRenderAttachmentInfo.eStencilFormat != 0)
    {
        pipelineDescriptor.stencilAttachmentPixelFormat = pl__metal_format(ptDescription->tRenderAttachmentInfo.eStencilFormat);
    }

    for(uint32_t i = 0; i < ptDescription->tRenderAttachmentInfo.uColorCount; i++)
    {
        pipelineDescriptor.colorAttachments[i].pixelFormat = pl__metal_format(ptDescription->tRenderAttachmentInfo.aeColorFormats[i]);
        pipelineDescriptor.colorAttachments[i].blendingEnabled = ptDescription->atBlendStates[i].bBlendEnabled ? YES : NO;
        pipelineDescriptor.colorAttachments[i].writeMask =
            (ptDescription->atBlendStates[i].uColorWriteMask & PL_COLOR_WRITE_MASK_R ? MTLColorWriteMaskRed : 0) |
            (ptDescription->atBlendStates[i].uColorWriteMask & PL_COLOR_WRITE_MASK_G ? MTLColorWriteMaskGreen : 0) |
            (ptDescription->atBlendStates[i].uColorWriteMask & PL_COLOR_WRITE_MASK_B ? MTLColorWriteMaskBlue : 0) |
            (ptDescription->atBlendStates[i].uColorWriteMask & PL_COLOR_WRITE_MASK_A ? MTLColorWriteMaskAlpha : 0);

        if(ptDescription->atBlendStates[i].bBlendEnabled)
        {
            pipelineDescriptor.colorAttachments[i].sourceRGBBlendFactor        = pl__metal_blend_factor(ptDescription->atBlendStates[i].eSrcColorFactor);
            pipelineDescriptor.colorAttachments[i].destinationRGBBlendFactor   = pl__metal_blend_factor(ptDescription->atBlendStates[i].eDstColorFactor);
            pipelineDescriptor.colorAttachments[i].rgbBlendOperation           = pl__metal_blend_op(ptDescription->atBlendStates[i].eColorOp);
            pipelineDescriptor.colorAttachments[i].sourceAlphaBlendFactor      = pl__metal_blend_factor(ptDescription->atBlendStates[i].eSrcAlphaFactor);
            pipelineDescriptor.colorAttachments[i].destinationAlphaBlendFactor = pl__metal_blend_factor(ptDescription->atBlendStates[i].eDstAlphaFactor);
            pipelineDescriptor.colorAttachments[i].alphaBlendOperation         = pl__metal_blend_op(ptDescription->atBlendStates[i].eAlphaOp);
        }
    }

    const plMetalShader tMetalShader = {
        .tDepthStencilState   = [ptDevice->tDevice newDepthStencilStateWithDescriptor:depthDescriptor],
        .tRenderPipelineState = [ptDevice->tDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error],
        .tCullMode            = pl__metal_cull(ptDescription->tGraphicsState.eCullMode),
        .tDepthClipMode       = ptDescription->tGraphicsState.bDepthClampEnabled ? MTLDepthClipModeClamp : MTLDepthClipModeClip
    };

    if (error != nil)
        NSLog(@"Error: failed to create Metal pipeline state: %@", error);
    
    ptMetalShader->tDepthStencilState = tMetalShader.tDepthStencilState;
    ptMetalShader->tRenderPipelineState = tMetalShader.tRenderPipelineState;
    ptMetalShader->tCullMode = tMetalShader.tCullMode;
    ptMetalShader->tDepthClipMode = tMetalShader.tDepthClipMode;

    return tHandle;
}

typedef struct _plInternalDeviceAllocatorData
{
    plDevice* ptDevice;
    plDeviceMemoryAllocatorI* ptAllocator;
} plInternalDeviceAllocatorData;

plDeviceMemoryAllocation
pl_allocate_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, uint32_t uTypeFilter, uint64_t ulSize, uint64_t ulAlignment, const char* pcName)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;

    plDeviceMemoryAllocation tAllocation = {
        .pHostMapped  = NULL,
        .uHandle      = UINT64_MAX,
        .ulOffset     = 0,
        .ulSize       = ulSize,
        .ptAllocator  = ptData->ptAllocator,
        .tMemoryFlags = PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT
    };

    plDeviceMemoryAllocation tBlock = pl_graphics_allocate_memory(ptData->ptDevice, ulSize, tAllocation.tMemoryFlags, uTypeFilter, "Uncached Heap");
    tAllocation.uHandle = tBlock.uHandle;
    tAllocation.pHostMapped = tBlock.pHostMapped;
    gptGraphics->szHostMemoryInUse += ulSize;
    return tAllocation;
}

void
pl_free_staging_dynamic(struct plDeviceMemoryAllocatorO* ptInst, plDeviceMemoryAllocation* ptAllocation)
{
    plInternalDeviceAllocatorData* ptData = (plInternalDeviceAllocatorData*)ptInst;
    plDeviceMemoryAllocation tBlock = {.uHandle = ptAllocation->uHandle};
    pl_graphics_free_memory(ptData->ptDevice, &tBlock);
    gptGraphics->szHostMemoryInUse -= ptAllocation->ulSize;
    ptAllocation->uHandle = UINT64_MAX;
    ptAllocation->ulSize = 0;
    ptAllocation->ulOffset = 0;
}

plGraphicsBackend
pl_graphics_get_backend(void)
{
    return PL_GRAPHICS_BACKEND_METAL;
}

const char*
pl_graphics_get_backend_string(void)
{
    return "Metal";
}

bool
pl_graphics_initialize(const plGraphicsInit* ptDesc)
{
    static plGraphics gtGraphics = {0};
    gptGraphics = &gtGraphics;
    gptDataRegistry->set_data("plGraphics", gptGraphics);
    gptGraphics->bValidationActive = ptDesc->eFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);

    // setup logging
    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_BUFFER | PL_LOG_CHANNEL_TYPE_CONSOLE,
        .uEntryCount = 1024
    };
    uLogChannelGraphics = gptLog->add_channel("Graphics", tLogInit);
    uint32_t uLogLevel = PL_LOG_LEVEL_INFO;
    #ifdef PL_CONFIG_DEBUG
        uLogLevel = PL_LOG_LEVEL_DEBUG;
    #endif
    gptLog->set_level(uLogChannelGraphics, uLogLevel);

    return true;
}

plSurface*
pl_graphics_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));
    return ptSurface;
}

void
pl_graphics_enumerate_devices(plDeviceInfo* atDeviceInfo, uint32_t* puDeviceCount)
{
    *puDeviceCount = 1;

    if(atDeviceInfo == NULL)
        return;

    plIO* ptIOCtx = gptIOI->get_io();
    id<MTLDevice> tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    strncpy(atDeviceInfo[0].acName, [tDevice.name UTF8String], 256);
    atDeviceInfo[0].eVendorId = PL_VENDOR_ID_APPLE;

    // printf("%s\n", [tDevice.architecture.name UTF8String]);
    atDeviceInfo[0].eType = PL_DEVICE_TYPE_INTEGRATED;
    atDeviceInfo[0].eCapabilities = PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING | PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY | PL_DEVICE_CAPABILITY_SWAPCHAIN | PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS;
    atDeviceInfo[0].tLimits.uNonCoherentAtomSize = 64;

    if(tDevice.hasUnifiedMemory)
    {
        atDeviceInfo[0].szHostMemory = tDevice.recommendedMaxWorkingSetSize;
    }
    else
    {
        atDeviceInfo[0].szDeviceMemory = tDevice.recommendedMaxWorkingSetSize;
    }

    if([tDevice supportsTextureSampleCount:64])      atDeviceInfo[0].eMaxSampleCount = 64;
    else if([tDevice supportsTextureSampleCount:32]) atDeviceInfo[0].eMaxSampleCount = 32;
    else if([tDevice supportsTextureSampleCount:16]) atDeviceInfo[0].eMaxSampleCount = 16;
    else if([tDevice supportsTextureSampleCount:8])  atDeviceInfo[0].eMaxSampleCount = 8;
    else if([tDevice supportsTextureSampleCount:4])  atDeviceInfo[0].eMaxSampleCount = 4;
    else if([tDevice supportsTextureSampleCount:2])  atDeviceInfo[0].eMaxSampleCount = 2;
    else atDeviceInfo[0].eMaxSampleCount = 1;
    
}

plDevice*
pl_graphics_create_device(const plDeviceInit* ptInit)
{
    plIO* ptIOCtx = gptIOI->get_io();

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));
    ptDevice->tInit = *ptInit;

    pl_sb_resize(ptDevice->sbuFreeHeaps, 64);
    for(uint64_t i = 0; i < 64; i++)
    {
        ptDevice->sbuFreeHeaps[i] = 63 - i;
        ptDevice->apcHeapNames[i] = "unnamed heap";
    }

    pl_sb_add(ptDevice->sbtShadersHot);
    pl_sb_add(ptDevice->sbtComputeShadersHot);
    pl_sb_add(ptDevice->sbtBuffersHot);
    pl_sb_add(ptDevice->sbtTexturesHot);
    pl_sb_add(ptDevice->sbtSamplersHot);
    pl_sb_add(ptDevice->sbtBindGroupsHot);
    pl_sb_add(ptDevice->sbtBindGroupLayoutsHot);
    
    pl_sb_add(ptDevice->sbtShadersCold);
    pl_sb_add(ptDevice->sbtComputeShadersCold);
    pl_sb_add(ptDevice->sbtBuffersCold);
    pl_sb_add(ptDevice->sbtTexturesCold);
    pl_sb_add(ptDevice->sbtSamplersCold);
    pl_sb_add(ptDevice->sbtBindGroupsCold);
    pl_sb_add(ptDevice->sbtBindGroupLayoutsCold);

    pl_sb_back(ptDevice->sbtShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtComputeShadersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBuffersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtTexturesCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtSamplersCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBindGroupsCold)._uGeneration = 1;
    pl_sb_back(ptDevice->sbtBindGroupLayoutsCold)._uGeneration = 1;

    ptDevice->tDevice = (__bridge id)ptIOCtx->pBackendPlatformData;

    uint32_t uDeviceCount = 16;
    plDeviceInfo atDeviceInfos[16] = {0};
    pl_graphics_enumerate_devices(atDeviceInfos, &uDeviceCount);

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

    pl_sb_resize(ptDevice->sbtGarbage, gptGraphics->uFramesInFlight + 1);
    plTempAllocator tTempAllocator = {0};
    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {
            .uNextValue = 0
        };

        tFrame.tFrameBoundaryEvent = [ptDevice->tDevice newSharedEvent];

        // pl_sb_resize(tFrame.sbtDynamicBuffers, 1);
        // static char atNameBuffer[PL_MAX_NAME_LENGTH] = {0};
        // pl_sprintf(atNameBuffer, "D-BUF-F%d-0", (int)i);
        // tFrame.sbtDynamicBuffers[0].tMemory = ptDevice->ptDynamicAllocator->allocate(ptDevice->ptDynamicAllocator->ptInst, 0, ptDevice->tInit.szDynamicBufferBlockSize, 0,atNameBuffer);
        // tFrame.sbtDynamicBuffers[0].tBuffer = [ptDevice->atHeaps[tFrame.sbtDynamicBuffers[0].tMemory.uHandle] newBufferWithLength:ptDevice->tInit.szDynamicBufferBlockSize options:MTLResourceStorageModeShared offset:0];
        // tFrame.sbtDynamicBuffers[0].tBuffer.label = [NSString stringWithUTF8String:pl_temp_allocator_sprintf(&tTempAllocator, "Dynamic Buffer: %u, 0", i)];
        pl_sb_push(ptDevice->sbtFrames, tFrame);
    }
    pl_temp_allocator_free(&tTempAllocator);

    MTL4ArgumentTableDescriptor* ptArgumentTableDescriptor = [MTL4ArgumentTableDescriptor new];
    ptArgumentTableDescriptor.maxBufferBindCount = 6;

    // Create the argument table.
    NSError *error = nil;
    ptDevice->tArgumentTable = [ptDevice->tDevice newArgumentTableWithDescriptor:ptArgumentTableDescriptor
                                                          error:&error];

    // Create all residency sets with the same default configuration.
    MTLResidencySetDescriptor *residencySetDescriptor;
    residencySetDescriptor = [MTLResidencySetDescriptor new];

    // Create a long-term residency set for resources that the app needs for every frame.
    ptDevice->tResidencySet = [ptDevice->tDevice newResidencySetWithDescriptor:residencySetDescriptor
                                                        error:&error];

    ptDevice->tCmdQueue4 = [ptDevice->tDevice newMTL4CommandQueue];
    [ptDevice->tCmdQueue4 addResidencySet:ptDevice->tResidencySet];

    return ptDevice;
}

plSwapchain*
pl_graphics_create_swapchain(plDevice* ptDevice, plSurface* ptSurface, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));

    ptSwap->tInfo.bVSync = ptInit->bVSync;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->ptSurface = ptSurface;
    ptSwap->ptDevice = ptDevice;
    ptSwap->uImageCount = gptGraphics->uFramesInFlight;
    ptSwap->tInfo.eFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptSwap->bVSync = true;

    ptSwap->tInfo.eSampleCount = pl_min(ptInit->eSampleCount, ptSwap->ptDevice->tInfo.eMaxSampleCount);
    if(ptSwap->tInfo.eSampleCount == 0)
        ptSwap->tInfo.eSampleCount = 1;


    for(uint32_t i = 0; i < ptSwap->uImageCount; i++)
    {
        plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
        plTexture* ptTexture = pl_graphics_get_texture(ptDevice, tHandle);
        pl_sb_push(ptSwap->sbtSwapchainTextureViews, tHandle);
        ptTexture->tDesc.tDimensions = (plVec3){gptIO->tMainViewportSize.x, gptIO->tMainViewportSize.y, 1.0f};
        ptTexture->tDesc.uLayers = 1;
        ptTexture->tDesc.uMips = 1;
        ptTexture->tDesc.eSampleCount = ptSwap->tInfo.eSampleCount;
        ptTexture->tDesc.eFormat = ptSwap->tInfo.eFormat;
        ptTexture->tDesc.eType = PL_TEXTURE_TYPE_2D;
        ptTexture->tDesc.eUsage = PL_TEXTURE_USAGE_PRESENT;
        ptTexture->tDesc.pcDebugName = "swapchain dummy image";
        ptTexture->tView.eFormat = ptTexture->tDesc.eFormat;
        ptTexture->tView.uBaseMip = 0;
        ptTexture->tView.uBaseLayer = 0;
        ptTexture->tView.uMips = 1;
        ptTexture->tView.uLayerCount = 1;
        ptTexture->tView.tTexture = tHandle;
        ptTexture->tView.tTexture = tHandle;
        ptTexture->tView.pcDebugName = "swapchain dummy image view";
    }
    ptSwap->tInfo.uWidth = (uint32_t)gptIO->tMainViewportSize.x;
    ptSwap->tInfo.uHeight = (uint32_t)gptIO->tMainViewportSize.y;
    pl_sb_resize(ptSwap->sbtSwapchainTextureViews, gptGraphics->uFramesInFlight);

    return ptSwap;
}

plTextureHandle*
pl_graphics_get_swapchain_images(plSwapchain* ptSwap, uint32_t* puSizeOut)
{
    if(puSizeOut)
        *puSizeOut = ptSwap->uImageCount;
    return ptSwap->sbtSwapchainTextureViews;
}

void
pl_graphics_recreate_swapchain(plSwapchain* ptSwap, const plSwapchainInit* ptInit)
{

    bool bMSAAChange = ptSwap->tInfo.eSampleCount != ptInit->eSampleCount;

    // gptGraphics->uCurrentFrameIndex = 0;
    ptSwap->tInfo.bVSync = ptInit->bVSync;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->tInfo.eSampleCount = pl_min(ptInit->eSampleCount, ptSwap->ptDevice->tInfo.eMaxSampleCount);
    if(ptSwap->tInfo.eSampleCount == 0)
        ptSwap->tInfo.eSampleCount = 1;
}

plCommandPool*
pl_graphics_create_command_pool(plDevice* ptDevice, const plCommandPoolDesc* ptDesc)
{
    plCommandPool* ptPool = PL_ALLOC(sizeof(plCommandPool));
    memset(ptPool, 0, sizeof(plCommandPool));

    ptPool->ptDevice = ptDevice;
    ptPool->tCmdAllocator = [ptDevice->tDevice newCommandAllocator];
    return ptPool;
}

void
pl_graphics_cleanup_command_pool(plCommandPool* ptPool)
{
    plCommandBuffer* ptCurrentCommandBuffer = ptPool->ptCommandBufferFreeList;
    while(ptCurrentCommandBuffer)
    {
        plCommandBuffer* ptNextCommandBuffer = ptCurrentCommandBuffer->ptNext;
        ptCurrentCommandBuffer->tEvent = nil;
        PL_FREE(ptCurrentCommandBuffer);
        ptCurrentCommandBuffer = ptNextCommandBuffer;
    }
    PL_FREE(ptPool);
}

void
pl_graphics_reset_command_pool(plCommandPool* ptPool, plCommandPoolResetFlags tFlags)
{
    [ptPool->tCmdAllocator reset];
}

void
pl_graphics_reset_command_buffer(plCommandBuffer* ptCommandBuffer)
{
}

plCommandBuffer*
pl_graphics_request_command_buffer(plCommandPool* ptPool, const char* pcDebugName)
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

    ptCommandBuffer->ptDevice = ptPool->ptDevice;
    ptCommandBuffer->ptPool = ptPool;
    ptCommandBuffer->ptNext = NULL;
    ptCommandBuffer->tCmdBuffer4 = [ptCommandBuffer->ptDevice->tDevice newCommandBuffer];

    char acName[64] = {0};
    pl_sprintf(acName, "%s (%llu)", pcDebugName, gptIO->ulFrameCount);
    ptCommandBuffer->tCmdBuffer4.label = [NSString stringWithUTF8String:acName];
    return ptCommandBuffer;
}

void
pl_graphics_begin_frame(plDevice* ptDevice)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    // Wait until the inflight command buffer has completed its work
    // gptGraphics->tSwapchain.uCurrentImageIndex = gptGraphics->uCurrentFrameIndex;
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    // ptFrame->uCurrentBufferIndex = 0;
    [ptFrame->tFrameBoundaryEvent waitUntilSignaledValue:ptFrame->uNextValue timeoutMS:10000];

    pl__garbage_collect(ptDevice); 
    [ptDevice->tResidencySet requestResidency];
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

bool
pl_graphics_acquire_swapchain_image(plSwapchain* ptSwapchain)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plDevice* ptDevice = ptSwapchain->ptDevice;

    plIO* ptIOCtx = gptIOI->get_io();
    gptGraphics->pMetalLayer = ptIOCtx->pBackendPlatformData;

    // get next drawable
    // if(ptSwapchain->tCurrentDrawable)
    //     [ptSwapchain->tCurrentDrawable release];

    ptSwapchain->uCurrentImageIndex = gptGraphics->uCurrentFrameIndex;
    ptSwapchain->tCurrentDrawable = [gptGraphics->pMetalLayer nextDrawable];
    if(ptSwapchain->tCurrentDrawable == nil)
    {
        PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
        return false;
    }

    // [ptSwapchain->tCurrentDrawable retain];

    plTextureHandle tTexture = ptSwapchain->sbtSwapchainTextureViews[gptGraphics->uCurrentFrameIndex];
    ptDevice->sbtTexturesHot[tTexture.uIndex].tTexture = ptSwapchain->tCurrentDrawable.texture;
    ptDevice->sbtTexturesHot[tTexture.uIndex].bSwapchain = true;

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

void
pl_graphics_begin_command_recording(plCommandBuffer* ptCommandBuffer)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    [ptDevice->tResidencySet requestResidency];

    [ptCommandBuffer->tCmdBuffer4 beginCommandBufferWithAllocator:ptCommandBuffer->ptPool->tCmdAllocator];
    if(gptGraphics->pMetalLayer)
    {
        [ptCommandBuffer->tCmdBuffer4 useResidencySet:gptGraphics->pMetalLayer.residencySet];
    }
}

void
pl_graphics_end_command_recording(plCommandBuffer* ptCommandBuffer)
{
    [ptCommandBuffer->tCmdBuffer4 endCommandBuffer];
}

bool
pl_graphics_present(plCommandBuffer* ptCommandBuffer, const plSubmitInfo* ptSubmitInfo, plSwapchain** ptSwaps, uint32_t uSwapchainCount)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;

    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    [ptDevice->tCmdQueue4 waitForDrawable:ptSwaps[0]->tCurrentDrawable];

    if (ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uWaitSemaphoreCount; i++)
        {
            if(ptSubmitInfo->atWaitSempahores[i]->tEvent)
            {
                [ptDevice->tCmdQueue4 waitForEvent:ptSubmitInfo->atWaitSempahores[i]->tEvent value:ptSubmitInfo->auWaitSemaphoreValues[i]];
            }
            else
            {
                [ptDevice->tCmdQueue4 waitForEvent:ptSubmitInfo->atWaitSempahores[i]->tSharedEvent value:ptSubmitInfo->auWaitSemaphoreValues[i]];
            }
        }
    }

    [ptDevice->tCmdQueue4 commit:&ptCommandBuffer->tCmdBuffer4 count:1];
    [ptDevice->tCmdQueue4 signalDrawable:ptSwaps[0]->tCurrentDrawable];
    
    [ptSwaps[0]->tCurrentDrawable present];

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {
            if(ptSubmitInfo->atSignalSempahores[i]->tEvent)
            {
                [ptDevice->tCmdQueue4 signalEvent:ptSubmitInfo->atSignalSempahores[i]->tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                [ptDevice->tCmdQueue4 signalEvent:ptSubmitInfo->atSignalSempahores[i]->tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }
    [ptDevice->tCmdQueue4 signalEvent:ptFrame->tFrameBoundaryEvent value:++ptFrame->uNextValue];
    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    ptDevice->sbtFrames[gptGraphics->uCurrentFrameIndex].uCurrentBufferIndex = 0;

    return true;
}

static void
pl__graphics_consumer_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    MTLStages tSrcStage = 0;
    MTLStages tDstStage = 0;
    MTL4VisibilityOptions tOptions = MTL4VisibilityOptionNone;

    if(tScope != 0) tOptions = MTL4VisibilityOptionDevice;

    if(tSrcStages & PL_PIPELINE_STAGE_COMPUTE)  tSrcStage |= MTLStageDispatch;
    if(tSrcStages & PL_PIPELINE_STAGE_BLIT)     tSrcStage |= MTLStageBlit;
    if(tSrcStages & PL_PIPELINE_STAGE_VERTEX)   tSrcStage |= MTLStageVertex;
    if(tSrcStages & PL_PIPELINE_STAGE_FRAGMENT) tSrcStage |= MTLStageFragment;

    if(tDstStages & PL_PIPELINE_STAGE_COMPUTE)  tDstStage |= MTLStageDispatch;
    if(tDstStages & PL_PIPELINE_STAGE_BLIT)     tDstStage |= MTLStageBlit;
    if(tDstStages & PL_PIPELINE_STAGE_VERTEX)   tDstStage |= MTLStageVertex;
    if(tDstStages & PL_PIPELINE_STAGE_FRAGMENT) tDstStage |= MTLStageFragment;

    if(gptGraphics->bComputeEncoderActive)
        [ptDevice->tComputeEncoder barrierAfterQueueStages:tSrcStage beforeStages:tDstStage visibilityOptions:tOptions];
    else if(gptGraphics->bRenderEncoderActive)
        [ptDevice->tRenderEncoder barrierAfterQueueStages:tSrcStage beforeStages:tDstStage visibilityOptions:tOptions];
}

void
pl_graphics_consumer_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    PL_ASSERT(!gptGraphics->bRenderEncoderActive && !gptGraphics->bComputeEncoderActive);
    PL_ASSERT(tSrcStages != 0);
    PL_ASSERT(tDstStages != 0);

    plStackedBarrier tBarrier = {
        .tSrcStages = tSrcStages,
        .tDstStages = tDstStages,
        .tScope = tScope
    };
    pl_sb_push(ptCmdBuffer->ptDevice->sbtBarrierStack, tBarrier);
}

void
pl_graphics_begin_render_pass(plCommandBuffer* ptCmdBuffer, const plRenderInfo* ptInfo, const plPassResources* ptResources)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    gptGraphics->bRenderEncoderActive = true;

    ptDevice->ptRenderPassDescriptor = [MTL4RenderPassDescriptor new];

    for(uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if(!pl_graphics_is_texture_valid(ptDevice, ptInfo->atColorAttachments[i].tTexture))
            break;

        ptDevice->ptRenderPassDescriptor.colorAttachments[i].texture = ptDevice->sbtTexturesHot[ptInfo->atColorAttachments[i].tTexture.uIndex].tTexture;
        ptDevice->ptRenderPassDescriptor.colorAttachments[i].loadAction = pl__metal_load_op(ptInfo->atColorAttachments[i].eLoadOp);
        ptDevice->ptRenderPassDescriptor.colorAttachments[i].storeAction = pl__metal_store_op(ptInfo->atColorAttachments[i].eStoreOp);
        ptDevice->ptRenderPassDescriptor.colorAttachments[i].slice = ptDevice->sbtTexturesCold[ptInfo->atColorAttachments[i].tTexture.uIndex].tView.uBaseLayer;

        ptDevice->ptRenderPassDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(
            ptInfo->atColorAttachments[i].tClearColor.r,
            ptInfo->atColorAttachments[i].tClearColor.g,
            ptInfo->atColorAttachments[i].tClearColor.b,
            ptInfo->atColorAttachments[i].tClearColor.a);

        if(ptInfo->atColorAttachments[i].tResolveTexture.uIndex > 0)
        {
            ptDevice->ptRenderPassDescriptor.colorAttachments[i].resolveTexture = ptDevice->sbtTexturesHot[ptInfo->atColorAttachments[i].tResolveTexture.uIndex].tTexture;
            ptDevice->ptRenderPassDescriptor.colorAttachments[i].storeAction = MTLStoreActionMultisampleResolve;
            
        }
    }

    if(pl_graphics_is_texture_valid(ptDevice, ptInfo->tDepthAttachment.tTexture))
    {
        ptDevice->ptRenderPassDescriptor.depthAttachment.loadAction = pl__metal_load_op(ptInfo->tDepthAttachment.eLoadOp);
        ptDevice->ptRenderPassDescriptor.depthAttachment.storeAction = pl__metal_store_op(ptInfo->tDepthAttachment.eStoreOp);
        ptDevice->ptRenderPassDescriptor.depthAttachment.clearDepth = ptInfo->tDepthAttachment.fClearZ;
        ptDevice->ptRenderPassDescriptor.depthAttachment.texture = ptDevice->sbtTexturesHot[ptInfo->tDepthAttachment.tTexture.uIndex].tTexture;
        ptDevice->ptRenderPassDescriptor.depthAttachment.slice = ptDevice->sbtTexturesCold[ptInfo->tDepthAttachment.tTexture.uIndex].tView.uBaseLayer;
    }

    if(pl_graphics_is_texture_valid(ptDevice, ptInfo->tStencilAttachment.tTexture))
    {
        ptDevice->ptRenderPassDescriptor.stencilAttachment.loadAction = pl__metal_load_op(ptInfo->tStencilAttachment.eLoadOp);
        ptDevice->ptRenderPassDescriptor.stencilAttachment.storeAction = pl__metal_store_op(ptInfo->tStencilAttachment.eStoreOp);
        ptDevice->ptRenderPassDescriptor.stencilAttachment.clearStencil = ptInfo->tStencilAttachment.uClearStencil;
        ptDevice->ptRenderPassDescriptor.stencilAttachment.texture = ptDevice->sbtTexturesHot[ptInfo->tStencilAttachment.tTexture.uIndex].tTexture;
        ptDevice->ptRenderPassDescriptor.stencilAttachment.slice = ptDevice->sbtTexturesCold[ptInfo->tStencilAttachment.tTexture.uIndex].tView.uBaseLayer;
    }

    ptDevice->tRenderEncoder = [ptCmdBuffer->tCmdBuffer4 renderCommandEncoderWithDescriptor:ptDevice->ptRenderPassDescriptor];
    // ptDevice->tRenderEncoder.label = [NSString stringWithUTF8String:ptRenderPass->tDesc.pcDebugName];

    for(uint32_t i = 0; i < PL_MAX_RENDER_TARGETS; i++)
    {
        if(!pl_graphics_is_texture_valid(ptDevice, ptInfo->atColorAttachments[i].tTexture))
            break;

        ptDevice->ptRenderPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionLoad;
    }

    if(pl_graphics_is_texture_valid(ptDevice, ptInfo->tDepthAttachment.tTexture))
    {
        ptDevice->ptRenderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
    }

    if(pl_graphics_is_texture_valid(ptDevice, ptInfo->tStencilAttachment.tTexture))
    {
        ptDevice->ptRenderPassDescriptor.stencilAttachment.loadAction = MTLLoadActionLoad;
    }

    while(pl_sb_size(ptDevice->sbtBarrierStack) > 0)
    {
        plStackedBarrier tBarrier = pl_sb_pop(ptDevice->sbtBarrierStack);
        pl__graphics_consumer_barrier(ptCmdBuffer, tBarrier.tSrcStages, tBarrier.tDstStages, tBarrier.tScope);
    }
}

void
pl_graphics_end_render_pass(plCommandBuffer* ptCmdBuffer)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    [ptDevice->tRenderEncoder endEncoding];
    ptDevice->tRenderEncoder = nil;
    gptGraphics->bRenderEncoderActive = false;
    [ptDevice->ptRenderPassDescriptor release];
    ptDevice->ptRenderPassDescriptor = nil;
}

void
pl_graphics_submit_command_buffer(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    id<MTL4CommandQueue> queue = ptDevice->tCmdQueue4;

    if (ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uWaitSemaphoreCount; i++)
        {
            if(ptSubmitInfo->atWaitSempahores[i]->tEvent)
            {
                [queue waitForEvent:ptSubmitInfo->atWaitSempahores[i]->tEvent value:ptSubmitInfo->auWaitSemaphoreValues[i]];
            }
            else
            {
                [queue waitForEvent:ptSubmitInfo->atWaitSempahores[i]->tSharedEvent value:ptSubmitInfo->auWaitSemaphoreValues[i]];
            }
        }
    }

    [queue commit:&ptCmdBuffer->tCmdBuffer4 count:1];

    plFrameContext* ptFrame = pl__get_frame_resources(ptCmdBuffer->ptDevice);
    ptCmdBuffer->uEventValue = ++ptFrame->uNextValue;
    ptCmdBuffer->tEvent = ptFrame->tFrameBoundaryEvent;

    if(ptSubmitInfo)
    {
        for(uint32_t i = 0; i < ptSubmitInfo->uSignalSemaphoreCount; i++)
        {

            if(ptSubmitInfo->atSignalSempahores[i]->tEvent)
            {
                // [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptSubmitInfo->atSignalSempahores[i]->tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
                [queue signalEvent:ptSubmitInfo->atSignalSempahores[i]->tEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
            else
            {
                // [ptCmdBuffer->tCmdBuffer encodeSignalEvent:ptSubmitInfo->atSignalSempahores[i]->tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
                [queue signalEvent:ptSubmitInfo->atSignalSempahores[i]->tSharedEvent value:ptSubmitInfo->auSignalSemaphoreValues[i]];
            }
        }
    }

    [queue signalEvent:ptFrame->tFrameBoundaryEvent value:ptCmdBuffer->uEventValue];
}

void
pl_graphics_wait_on_command_buffer(plCommandBuffer* ptCmdBuffer)
{
    if(ptCmdBuffer->tEvent)
    {
        [(id<MTLSharedEvent>)ptCmdBuffer->tEvent
            waitUntilSignaledValue:ptCmdBuffer->uEventValue
                          timeoutMS:UINT64_MAX];
    }
}

void
pl_graphics_return_command_buffer(plCommandBuffer* ptCmdBuffer)
{
    
    ptCmdBuffer->tCmdBuffer4.label = nil;
    [ptCmdBuffer->tCmdBuffer4 release];
    ptCmdBuffer->tCmdBuffer4 = nil;
    ptCmdBuffer->ptNext = ptCmdBuffer->ptPool->ptCommandBufferFreeList;
    ptCmdBuffer->ptPool->ptCommandBufferFreeList = ptCmdBuffer;
}

void
pl_graphics_begin_compute_pass(plCommandBuffer* ptCmdBuffer, const plPassResources* ptResources)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    ptDevice->tComputeEncoder = [ptCmdBuffer->tCmdBuffer4 computeCommandEncoder];
    gptGraphics->bComputeEncoderActive = true;
}

void
pl_graphics_end_compute_pass(plCommandBuffer* ptCmdBuffer)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    [ptDevice->tComputeEncoder endEncoding];
    // [ptDevice->tComputeEncoder release];
    ptDevice->tComputeEncoder = nil;
    gptGraphics->bComputeEncoderActive = false;
}

void
pl_graphics_intra_pass_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope, const plPassResources* ptResources)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    PL_ASSERT(gptGraphics->bRenderEncoderActive || gptGraphics->bComputeEncoderActive);
    PL_ASSERT(tSrcStages != 0);
    PL_ASSERT(tDstStages != 0);

    MTLStages tSrcStage = 0;
    MTLStages tDstStage = 0;
    MTL4VisibilityOptions tOptions = MTL4VisibilityOptionNone;

    if(tScope != 0) tOptions = MTL4VisibilityOptionDevice;

    if(tSrcStages & PL_PIPELINE_STAGE_COMPUTE)  tSrcStage |= MTLStageDispatch;
    if(tSrcStages & PL_PIPELINE_STAGE_BLIT)     tSrcStage |= MTLStageBlit;
    if(tSrcStages & PL_PIPELINE_STAGE_VERTEX)   tSrcStage |= MTLStageVertex;
    if(tSrcStages & PL_PIPELINE_STAGE_FRAGMENT) tSrcStage |= MTLStageFragment;

    if(tDstStages & PL_PIPELINE_STAGE_COMPUTE)  tDstStage |= MTLStageDispatch;
    if(tDstStages & PL_PIPELINE_STAGE_BLIT)     tDstStage |= MTLStageBlit;
    if(tDstStages & PL_PIPELINE_STAGE_VERTEX)   tDstStage |= MTLStageVertex;
    if(tDstStages & PL_PIPELINE_STAGE_FRAGMENT) tDstStage |= MTLStageFragment;

    if(gptGraphics->bComputeEncoderActive)
        [ptDevice->tComputeEncoder barrierAfterEncoderStages:tSrcStage beforeEncoderStages:tDstStage visibilityOptions:tOptions];
    else if(gptGraphics->bRenderEncoderActive)
    {
        if(tSrcStage & MTLStageFragment)
        {
            [ptDevice->tRenderEncoder barrierAfterStages:tSrcStage beforeQueueStages:tDstStage visibilityOptions:tOptions];
            [ptDevice->tRenderEncoder endEncoding];
            ptDevice->tRenderEncoder = nil;
            ptDevice->tRenderEncoder = [ptCmdBuffer->tCmdBuffer4 renderCommandEncoderWithDescriptor:ptDevice->ptRenderPassDescriptor];
            [ptDevice->tRenderEncoder barrierAfterQueueStages:tSrcStage beforeStages:tDstStage visibilityOptions:tOptions];

        }
        else
            [ptDevice->tComputeEncoder barrierAfterEncoderStages:tSrcStage beforeEncoderStages:tDstStage visibilityOptions:tOptions];
    }
}

MTL4RenderPassDescriptor*
pl_graphics_get_metal_render_pass_descriptor(plCommandBuffer* ptCmdBuffer)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    return ptDevice->ptRenderPassDescriptor;
}

void
pl_graphics_producer_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    PL_ASSERT(gptGraphics->bRenderEncoderActive || gptGraphics->bComputeEncoderActive);
    PL_ASSERT(tSrcStages != 0);
    PL_ASSERT(tDstStages != 0);

    MTLStages tSrcStage = 0;
    MTLStages tDstStage = 0;
    MTL4VisibilityOptions tOptions = MTL4VisibilityOptionNone;

    if(tScope != 0) tOptions = MTL4VisibilityOptionDevice;

    if(tSrcStages & PL_PIPELINE_STAGE_COMPUTE)  tSrcStage |= MTLStageDispatch;
    if(tSrcStages & PL_PIPELINE_STAGE_BLIT)     tSrcStage |= MTLStageBlit;
    if(tSrcStages & PL_PIPELINE_STAGE_VERTEX)   tSrcStage |= MTLStageVertex;
    if(tSrcStages & PL_PIPELINE_STAGE_FRAGMENT) tSrcStage |= MTLStageFragment;

    if(tDstStages & PL_PIPELINE_STAGE_COMPUTE)  tDstStage |= MTLStageDispatch;
    if(tDstStages & PL_PIPELINE_STAGE_BLIT)     tDstStage |= MTLStageBlit;
    if(tDstStages & PL_PIPELINE_STAGE_VERTEX)   tDstStage |= MTLStageVertex;
    if(tDstStages & PL_PIPELINE_STAGE_FRAGMENT) tDstStage |= MTLStageFragment;

    if(gptGraphics->bComputeEncoderActive)
        [ptDevice->tComputeEncoder barrierAfterStages:tSrcStage beforeQueueStages:tDstStage visibilityOptions:tOptions];
    else if(gptGraphics->bRenderEncoderActive)
        [ptDevice->tRenderEncoder barrierAfterStages:tSrcStage beforeQueueStages:tDstStage visibilityOptions:tOptions];
}

void
pl_graphics_dispatch(plCommandBuffer* ptCmdBuffer, uint32_t uDispatchCount, const plDispatch* atDispatches)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    [ptDevice->tComputeEncoder setArgumentTable:ptDevice->tArgumentTable];
    for(uint32_t i = 0; i < uDispatchCount; i++)
    {
        const plDispatch* ptDispatch = &atDispatches[i];
        MTLSize tGridSize = MTLSizeMake(ptDispatch->uGroupCountX, ptDispatch->uGroupCountY, ptDispatch->uGroupCountZ);
        MTLSize tThreadsPerGroup = MTLSizeMake(ptDispatch->uThreadPerGroupX, ptDispatch->uThreadPerGroupY, ptDispatch->uThreadPerGroupZ);
        [ptDevice->tComputeEncoder dispatchThreadgroups:tGridSize threadsPerThreadgroup:tThreadsPerGroup];
    }
}

void
pl_graphics_bind_compute_bind_groups(plCommandBuffer* ptCmdBuffer, plComputeShaderHandle tHandle, uint32_t uFirst, uint32_t uCount,
    const plBindGroupHandle* atBindGroups, uint32_t uDynamicCount, const plDynamicBinding* ptDynamicBinding)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(uDynamicCount > 0)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
        // [ptDevice->tComputeEncoder setBuffer:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer offset:ptDynamicBinding->uByteOffset atIndex:3];
        [ptDevice->tArgumentTable setAddress:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer.gpuAddress+(uint64_t)ptDynamicBinding->uByteOffset atIndex:3];
    }

    for(uint32_t i = 0; i < uCount; i++)
    {
        plBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsCold[atBindGroups[i].uIndex];
        plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];
        [ptDevice->tArgumentTable setAddress:ptMetalBindGroup->tShaderArgumentBuffer.gpuAddress+(uint64_t)ptMetalBindGroup->uOffset atIndex:uFirst+i];
    }
    [ptDevice->tComputeEncoder setArgumentTable:ptCmdBuffer->ptDevice->tArgumentTable];
}

void
pl_graphics_bind_graphics_bind_groups(plCommandBuffer* ptCmdBuffer, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle* atBindGroups, uint32_t uDynamicCount, const plDynamicBinding* ptDynamicBinding)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    if(uDynamicCount > 0)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
        [ptDevice->tArgumentTable setAddress:ptFrame->sbtDynamicBuffers[ptDynamicBinding->uBufferHandle].tBuffer.gpuAddress+(uint64_t)ptDynamicBinding->uByteOffset atIndex:3];
    }

    for(uint32_t i = 0; i < uCount; i++)
    {
        plBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsCold[atBindGroups[i].uIndex];
        plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];
        [ptDevice->tArgumentTable setAddress:ptMetalBindGroup->tShaderArgumentBuffer.gpuAddress+(uint64_t)ptMetalBindGroup->uOffset atIndex:uFirst + i];
    }
}

void
pl_graphics_set_depth_bias(plCommandBuffer* ptCmdBuffer, float fDepthBiasConstantFactor, float fDepthBiasClamp, float fDepthBiasSlopeFactor)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    [ptDevice->tRenderEncoder setDepthBias:fDepthBiasConstantFactor slopeScale:fDepthBiasSlopeFactor clamp:fDepthBiasClamp];
}

void
pl_graphics_set_viewport(plCommandBuffer* ptCmdBuffer, const plRenderViewport* ptViewport)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    MTLViewport tViewport = {
        .originX = ptViewport->fX,
        .originY = ptViewport->fY,
        .width   = ptViewport->fWidth,
        .height  = ptViewport->fHeight,
        .znear   = ptViewport->fMinDepth,
        .zfar    = ptViewport->fMaxDepth
    };
    [ptDevice->tRenderEncoder setViewport:tViewport];
}

void
pl_graphics_set_scissor_region(plCommandBuffer* ptCmdBuffer, const plScissor* ptScissor)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    MTLScissorRect tScissorRect = {
        .x      = (NSUInteger)(ptScissor->iOffsetX),
        .y      = (NSUInteger)(ptScissor->iOffsetY),
        .width  = (NSUInteger)(ptScissor->uWidth),
        .height = (NSUInteger)(ptScissor->uHeight)
    };
    [ptDevice->tRenderEncoder setScissorRect:tScissorRect];
}

void
pl_graphics_bind_vertex_buffer(plCommandBuffer* ptCmdBuffer, plBufferHandle tHandle)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    // [ptDevice->tRenderEncoder setVertexBuffer:ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer offset:0 atIndex:4];
    [ptDevice->tArgumentTable setAddress:ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer.gpuAddress atIndex:4];
}

void
pl_graphics_bind_vertex_buffers(plCommandBuffer* ptCmdBuffer, uint32_t uFirst, uint32_t uCount, const plBufferHandle* ptHandles, const size_t* pszOffsets)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;

    for(uint32_t i = 0; i < uCount; i++)
    {
        // [ptDevice->tRenderEncoder setVertexBuffer:ptDevice->sbtBuffersHot[ptHandles[i].uIndex].tBuffer offset:(pszOffsets == NULL) ? 0 : pszOffsets[i] atIndex:4 + uFirst + i];
        [ptDevice->tArgumentTable setAddress:ptDevice->sbtBuffersHot[ptHandles[i].uIndex].tBuffer.gpuAddress+((pszOffsets == NULL) ? 0 : pszOffsets[i]) atIndex:4 + uFirst + i];
    }
}

void
pl_graphics_draw(plCommandBuffer* ptCmdBuffer, uint32_t uCount, const plDraw* atDraws)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    // [ptDevice->tResidencySet commit];
    [ptDevice->tRenderEncoder setArgumentTable:ptDevice->tArgumentTable atStages:MTLRenderStageVertex | MTLRenderStageFragment];
    for(uint32_t i = 0; i < uCount; i++)
    {
        [ptDevice->tRenderEncoder drawPrimitives:MTLPrimitiveTypeTriangle 
            vertexStart:atDraws[i].uVertexStart
            vertexCount:atDraws[i].uVertexCount
            instanceCount:atDraws[i].uInstanceCount
            baseInstance:atDraws[i].uInstance
            ];
    }
}

void
pl_graphics_draw_indexed(plCommandBuffer* ptCmdBuffer, uint32_t uCount, const plDrawIndex* atDraws)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    // [ptDevice->tResidencySet commit];
    [ptDevice->tRenderEncoder setArgumentTable:ptDevice->tArgumentTable atStages:MTLRenderStageVertex | MTLRenderStageFragment];
    for(uint32_t i = 0; i < uCount; i++)
    {
        [ptDevice->tRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
            indexCount:atDraws[i].uIndexCount
            indexType:MTLIndexTypeUInt32
            indexBuffer:ptDevice->sbtBuffersHot[atDraws[i].tIndexBuffer.uIndex].tBuffer.gpuAddress + atDraws[i].uIndexStart * sizeof(uint32_t)
            indexBufferLength:ptDevice->sbtBuffersCold[atDraws[i].tIndexBuffer.uIndex].tMemoryRequirements.ulSize
            instanceCount:atDraws[i].uInstanceCount
            baseVertex:atDraws[i].uVertexStart
            baseInstance:atDraws[i].uInstance
            ];
    }
}

void
pl_graphics_bind_shader(plCommandBuffer* ptCmdBuffer, plShaderHandle tHandle)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[tHandle.uIndex];

    [ptDevice->tRenderEncoder setStencilReferenceValue:ptMetalShader->ulStencilRef];
    [ptDevice->tRenderEncoder setCullMode:ptMetalShader->tCullMode];
    [ptDevice->tRenderEncoder setDepthClipMode:ptMetalShader->tDepthClipMode];
    [ptDevice->tRenderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [ptDevice->tRenderEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
    [ptDevice->tRenderEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
    [ptDevice->tRenderEncoder setTriangleFillMode:ptMetalShader->tFillMode];
}

void
pl_graphics_bind_compute_shader(plCommandBuffer* ptCmdBuffer, plComputeShaderHandle tHandle)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plMetalComputeShader* ptMetalShader = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    // NSLog(@"maxTotalThreadsPerThreadgroup=%lu threadExecutionWidth=%lu",
    //     (unsigned long)ptMetalShader->tPipelineState.maxTotalThreadsPerThreadgroup,
    //     (unsigned long)ptMetalShader->tPipelineState.threadExecutionWidth);
    [ptDevice->tComputeEncoder setComputePipelineState:ptMetalShader->tPipelineState];
}

void
pl_graphics_draw_stream(plCommandBuffer* ptCmdBuffer, uint32_t uAreaCount, plDrawArea* atAreas)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);

    for(uint32_t i = 0; i < uAreaCount; i++)
    {
        plDrawArea* ptArea = &atAreas[i];
        plDrawStream* ptStream = ptArea->ptDrawStream;

        MTLScissorRect atScissors[PL_MAX_VIEWPORTS] = {0};
        MTLViewport atViewports[PL_MAX_VIEWPORTS] = {0};

        uint32_t uViewportCount = 0;

        for(uint32_t j = 0; j < PL_MAX_VIEWPORTS; j++)
        {

            if(ptArea->atViewports[j].fWidth == 0.0f)
            {
                break;
            }

            atScissors[j] = (MTLScissorRect){
                .x      = (NSUInteger)(ptArea->atScissors[j].iOffsetX),
                .y      = (NSUInteger)(ptArea->atScissors[j].iOffsetY),
                .width  = (NSUInteger)(ptArea->atScissors[j].uWidth),
                .height = (NSUInteger)(ptArea->atScissors[j].uHeight)
            };

            atViewports[j] = (MTLViewport){
                .originX = ptArea->atViewports[j].fX,
                .originY = ptArea->atViewports[j].fY,
                .width   = ptArea->atViewports[j].fWidth,
                .height  = ptArea->atViewports[j].fHeight,
                .znear   = 0,
                .zfar    = 1.0
            };

            uViewportCount++;
        }

        [ptDevice->tRenderEncoder setViewports:atViewports count:uViewportCount];
        [ptDevice->tRenderEncoder setScissorRects:atScissors count:uViewportCount];

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
        uint32_t uDynamicBufferIndex = UINT32_MAX;
        while(uCurrentStreamIndex < uTokens)
        {
            const uint32_t uDirtyMask = ptStream->_auStream[uCurrentStreamIndex];
            uCurrentStreamIndex++;

            [ptDevice->tRenderEncoder setArgumentTable:ptDevice->tArgumentTable atStages:MTLRenderStageVertex | MTLRenderStageFragment];

            if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
            {
                const plShaderHandle tShaderHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                const plShader* ptShader= &ptDevice->sbtShadersCold[tShaderHandle.uIndex];
                plMetalShader* ptMetalShader = &ptDevice->sbtShadersHot[tShaderHandle.uIndex];
                [ptDevice->tRenderEncoder setCullMode:ptMetalShader->tCullMode];
                [ptDevice->tRenderEncoder setDepthClipMode:ptMetalShader->tDepthClipMode];
                [ptDevice->tRenderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
                if(tCurrentDepthStencilState != ptMetalShader->tDepthStencilState)
                {
                    [ptDevice->tRenderEncoder setDepthStencilState:ptMetalShader->tDepthStencilState];
                }
                tCurrentDepthStencilState = ptMetalShader->tDepthStencilState;
                [ptDevice->tRenderEncoder setRenderPipelineState:ptMetalShader->tRenderPipelineState];
                [ptDevice->tRenderEncoder setTriangleFillMode:ptMetalShader->tFillMode];
                [ptDevice->tRenderEncoder setStencilReferenceValue:ptMetalShader->ulStencilRef];                

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
                plBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsCold[tBindGroupHandle.uIndex];
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];
                [ptDevice->tArgumentTable setAddress:ptMetalBindGroup->tShaderArgumentBuffer.gpuAddress+(uint64_t)ptMetalBindGroup->uOffset atIndex:0];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];
                plBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsCold[tBindGroupHandle.uIndex];
                [ptDevice->tArgumentTable setAddress:ptMetalBindGroup->tShaderArgumentBuffer.gpuAddress+(uint64_t)ptMetalBindGroup->uOffset atIndex:1];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
            {
                const plBindGroupHandle tBindGroupHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                plMetalBindGroup* ptMetalBindGroup = &ptDevice->sbtBindGroupsHot[tBindGroupHandle.uIndex];
                plBindGroup* ptBindGroup = &ptDevice->sbtBindGroupsCold[tBindGroupHandle.uIndex];
                [ptDevice->tArgumentTable setAddress:ptMetalBindGroup->tShaderArgumentBuffer.gpuAddress+(uint64_t)ptMetalBindGroup->uOffset atIndex:2];
                uCurrentStreamIndex++;
            }

            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER_0)
            {
                uDynamicBufferIndex = ptStream->_auStream[uCurrentStreamIndex];
                [ptDevice->tArgumentTable setAddress:ptFrame->sbtDynamicBuffers[uDynamicBufferIndex].tBuffer.gpuAddress+(uint64_t)uDynamicBufferOffset0 atIndex:3];

                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET_0)
            {
                [ptDevice->tArgumentTable setAddress:ptFrame->sbtDynamicBuffers[uDynamicBufferIndex].tBuffer.gpuAddress+(uint64_t)uDynamicBufferOffset0 atIndex:3];
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
                [ptDevice->tArgumentTable setAddress:ptDevice->sbtBuffersHot[tBufferHandle.uIndex].tBuffer.gpuAddress atIndex:4];
                uCurrentStreamIndex++;
            }
            if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER_1)
            {
                const plBufferHandle tBufferHandle = {.uData = ptStream->_auStream[uCurrentStreamIndex] };
                [ptDevice->tArgumentTable setAddress:ptDevice->sbtBuffersHot[tBufferHandle.uIndex].tBuffer.gpuAddress atIndex:5];
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

            [ptDevice->tRenderEncoder setArgumentTable:ptDevice->tArgumentTable atStages:MTLRenderStageVertex | MTLRenderStageFragment];

            if(tIndexBuffer.uData == 0)
            {
                [ptDevice->tRenderEncoder drawPrimitives:MTLPrimitiveTypeTriangle 
                    vertexStart:uVertexBufferOffset
                    vertexCount:uTriangleCount * 3
                    instanceCount:uInstanceCount
                    baseInstance:uInstanceStart
                    ];
            }
            else
            {
                [ptDevice->tRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle 
                    indexCount:uTriangleCount * 3
                    indexType:MTLIndexTypeUInt32
                    indexBuffer:ptDevice->sbtBuffersHot[tIndexBuffer.uIndex].tBuffer.gpuAddress + (uint64_t)uIndexBufferOffset * sizeof(uint32_t)
                    indexBufferLength:ptDevice->sbtBuffersCold[tIndexBuffer.uIndex].tMemoryRequirements.ulSize
                    instanceCount:uInstanceCount
                    baseVertex:uVertexBufferOffset
                    baseInstance:uInstanceStart
                    ];
            }
        }
    }
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_graphics_flush_device(plDevice* ptDevice)
{
    gptThreads->sleep_thread(500);
}

void
pl_graphics_cleanup(void)
{
    pl__cleanup_common_graphics();
}

void
pl_graphics_cleanup_surface(plSurface* ptSurface)
{
    PL_FREE(ptSurface);
}

void
pl_graphics_cleanup_swapchain(plSwapchain* ptSwap)
{
    pl__cleanup_common_swapchain(ptSwap);
}

void
pl_graphics_cleanup_device(plDevice* ptDevice)
{
    pl_sb_free(ptDevice->sbuFreeHeaps);
    pl_sb_free(ptDevice->sbtTexturesHot);
    pl_sb_free(ptDevice->sbtSamplersHot);
    pl_sb_free(ptDevice->sbtBindGroupsHot);
    pl_sb_free(ptDevice->sbtBuffersHot);
    pl_sb_free(ptDevice->sbtShadersHot);
    pl_sb_free(ptDevice->sbtBindGroupLayoutsHot);
    pl_sb_free(ptDevice->sbtComputeShadersHot);

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtFrames); i++)
    {
        plFrameContext* ptFrame = &ptDevice->sbtFrames[i];
        pl_sb_free(ptFrame->sbtDynamicBuffers);
    }
    pl_sb_free(ptDevice->sbtFrames);

    pl__cleanup_common_device(ptDevice);
}

void
pl_graphics_insert_debug_label(plCommandBuffer* ptCmdBuffer, const char* pcLabel, plVec4 tColor)
{
}

void
pl_graphics_push_debug_group(plCommandBuffer* ptCmdBuffer, const char* pcLabel, plVec4 tColor)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    if(ptDevice->tComputeEncoder)
    {
        [ptDevice->tComputeEncoder pushDebugGroup:[NSString stringWithUTF8String:pcLabel]];
    }
    else if(ptDevice->tRenderEncoder)
    {
        [ptDevice->tRenderEncoder pushDebugGroup:[NSString stringWithUTF8String:pcLabel]];
    }
    else
    {
        [ptCmdBuffer->tCmdBuffer4 pushDebugGroup:[NSString stringWithUTF8String:pcLabel]];
    }
}

void
pl_graphics_pop_debug_group(plCommandBuffer* ptCmdBuffer)
{
    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    if(ptDevice->tComputeEncoder)
    {
        [ptDevice->tComputeEncoder popDebugGroup];
    }
    else if(ptDevice->tRenderEncoder)
    {
        [ptDevice->tRenderEncoder popDebugGroup];
    }
    else
    {
        [ptCmdBuffer->tCmdBuffer4 popDebugGroup];
    }
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
pl__is_depth_format(plFormat eFormat)
{
    switch(eFormat)
    {
        case PL_FORMAT_D32_FLOAT:
        case PL_FORMAT_D32_FLOAT_S8_UINT:
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D16_UNORM_S8_UINT: return true;
    }
    return false;
}

static bool
pl__is_stencil_format(plFormat eFormat)
{
    switch(eFormat)
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
pl__metal_data_type(plDataType eType)
{
    switch(eType)
    {

        case PL_DATA_TYPE_BOOL:           return MTLDataTypeBool;
        case PL_DATA_TYPE_FLOAT:          return MTLDataTypeFloat;
        case PL_DATA_TYPE_UCHAR:  return MTLDataTypeUChar;
        case PL_DATA_TYPE_CHAR:           return MTLDataTypeChar;
        case PL_DATA_TYPE_USHORT: return MTLDataTypeUShort;
        case PL_DATA_TYPE_SHORT:          return MTLDataTypeShort;
        case PL_DATA_TYPE_UINT:   return MTLDataTypeUInt;
        case PL_DATA_TYPE_INT:            return MTLDataTypeInt;

        case PL_DATA_TYPE_BOOL2:           return MTLDataTypeBool2;
        case PL_DATA_TYPE_FLOAT2:          return MTLDataTypeFloat2;
        case PL_DATA_TYPE_UCHAR2:  return MTLDataTypeUChar2;
        case PL_DATA_TYPE_CHAR2:           return MTLDataTypeChar2;
        case PL_DATA_TYPE_USHORT2: return MTLDataTypeUShort2;
        case PL_DATA_TYPE_SHORT2:          return MTLDataTypeShort2;
        case PL_DATA_TYPE_UINT2:   return MTLDataTypeUInt2;
        case PL_DATA_TYPE_INT2:            return MTLDataTypeInt2;

        case PL_DATA_TYPE_BOOL3:           return MTLDataTypeBool3;
        case PL_DATA_TYPE_FLOAT3:          return MTLDataTypeFloat3;
        case PL_DATA_TYPE_UCHAR3:  return MTLDataTypeUChar3;
        case PL_DATA_TYPE_CHAR3:           return MTLDataTypeChar3;
        case PL_DATA_TYPE_USHORT3: return MTLDataTypeUShort3;
        case PL_DATA_TYPE_SHORT3:          return MTLDataTypeShort3;
        case PL_DATA_TYPE_UINT3:   return MTLDataTypeUInt3;
        case PL_DATA_TYPE_INT3:            return MTLDataTypeInt3;

        case PL_DATA_TYPE_BOOL4:           return MTLDataTypeBool4;
        case PL_DATA_TYPE_FLOAT4:          return MTLDataTypeFloat4;
        case PL_DATA_TYPE_UCHAR4:  return MTLDataTypeUChar4;
        case PL_DATA_TYPE_CHAR4:           return MTLDataTypeChar4;
        case PL_DATA_TYPE_USHORT4: return MTLDataTypeUShort4;
        case PL_DATA_TYPE_SHORT4:          return MTLDataTypeShort4;
        case PL_DATA_TYPE_UINT4:   return MTLDataTypeUInt4;
        case PL_DATA_TYPE_INT4:            return MTLDataTypeInt4;
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
        case PL_ADDRESS_MODE_WRAP:            return MTLSamplerAddressModeRepeat;
        case PL_ADDRESS_MODE_CLAMP_TO_EDGE:   return MTLSamplerAddressModeClampToEdge;
        case PL_ADDRESS_MODE_MIRROR:          return MTLSamplerAddressModeMirrorRepeat;
        case PL_ADDRESS_MODE_CLAMP_TO_BORDER: return MTLSamplerAddressModeClampToBorderColor;
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
pl__metal_format(plFormat eFormat)
{
    switch(eFormat)
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
        case PL_FORMAT_D16_UNORM:           return MTLPixelFormatDepth16Unorm;
        case PL_FORMAT_R8_SNORM:            return MTLPixelFormatR8Unorm;
        case PL_FORMAT_R8_UINT:             return MTLPixelFormatR8Uint; 
        case PL_FORMAT_R8_SINT:             return MTLPixelFormatR8Sint;
        case PL_FORMAT_R8_SRGB:             return MTLPixelFormatR8Unorm_sRGB;
        case PL_FORMAT_R16_UNORM:           return MTLPixelFormatR16Unorm; 
        case PL_FORMAT_R16_SNORM:           return MTLPixelFormatR16Snorm;
        case PL_FORMAT_R16_UINT:            return MTLPixelFormatR16Uint;
        case PL_FORMAT_R16_SINT:            return MTLPixelFormatR16Sint;
        case PL_FORMAT_R16_FLOAT:           return MTLPixelFormatR16Float;
        case PL_FORMAT_R8G8_SNORM:          return MTLPixelFormatRG8Snorm;
        case PL_FORMAT_R8G8_UINT:           return MTLPixelFormatRG8Uint;
        case PL_FORMAT_R8G8_SINT:           return MTLPixelFormatRG8Sint;
        case PL_FORMAT_R8G8_SRGB:           return MTLPixelFormatRG8Unorm_sRGB;
        case PL_FORMAT_B5G6R5_UNORM:        return MTLPixelFormatB5G6R5Unorm;
        case PL_FORMAT_A1R5G5B5_UNORM:      return MTLPixelFormatA1BGR5Unorm;
        case PL_FORMAT_B5G5R5A1_UNORM:      return MTLPixelFormatBGR5A1Unorm;
        case PL_FORMAT_R32_SINT:            return MTLPixelFormatR32Sint;
        case PL_FORMAT_R32_FLOAT:           return MTLPixelFormatR32Float;
        case PL_FORMAT_R16G16_UNORM:        return MTLPixelFormatRG16Unorm;
        case PL_FORMAT_R16G16_SNORM:        return MTLPixelFormatRG16Snorm;
        case PL_FORMAT_R16G16_UINT:         return MTLPixelFormatRG16Uint;
        case PL_FORMAT_R16G16_SINT:         return MTLPixelFormatRG16Sint;
        case PL_FORMAT_R16G16_FLOAT:        return MTLPixelFormatRG16Float;
        case PL_FORMAT_R8G8B8A8_SNORM:      return MTLPixelFormatRGBA8Snorm;
        case PL_FORMAT_R8G8B8A8_UINT:       return MTLPixelFormatRGBA8Uint;
        case PL_FORMAT_R8G8B8A8_SINT:       return MTLPixelFormatRGBA8Sint;
        case PL_FORMAT_B10G10R10A2_UNORM:   return MTLPixelFormatBGR10A2Unorm;
        case PL_FORMAT_R10G10B10A2_UNORM:   return MTLPixelFormatRGB10A2Unorm;
        case PL_FORMAT_R10G10B10A2_UINT:    return MTLPixelFormatRGB10A2Uint;
        case PL_FORMAT_R11G11B10_FLOAT:     return MTLPixelFormatRG11B10Float;
        case PL_FORMAT_R9G9B9E5_FLOAT:      return MTLPixelFormatRGB9E5Float;
        case PL_FORMAT_R32G32_UINT:         return MTLPixelFormatRG32Uint;
        case PL_FORMAT_R32G32_SINT:         return MTLPixelFormatRG32Sint;
        case PL_FORMAT_R16G16B16A16_UNORM:  return MTLPixelFormatRGBA16Unorm;
        case PL_FORMAT_R16G16B16A16_SNORM:  return MTLPixelFormatRGBA16Snorm;
        case PL_FORMAT_R16G16B16A16_UINT:   return MTLPixelFormatRGBA16Uint;
        case PL_FORMAT_R16G16B16A16_SINT:   return MTLPixelFormatRGBA16Sint;
        case PL_FORMAT_R16G16B16A16_FLOAT:  return MTLPixelFormatRGBA16Float;
        case PL_FORMAT_R32G32B32A32_UINT:   return MTLPixelFormatRGBA32Uint;
        case PL_FORMAT_R32G32B32A32_SINT:   return MTLPixelFormatRGBA32Sint;
        case PL_FORMAT_BC1_RGBA_UNORM:      return MTLPixelFormatBC1_RGBA;
        case PL_FORMAT_BC1_RGBA_SRGB:       return MTLPixelFormatBC1_RGBA_sRGB;
        case PL_FORMAT_BC2_UNORM:           return MTLPixelFormatBC2_RGBA;
        case PL_FORMAT_BC2_SRGB:            return MTLPixelFormatBC2_RGBA_sRGB;
        case PL_FORMAT_BC3_UNORM:           return MTLPixelFormatBC3_RGBA;
        case PL_FORMAT_BC3_SRGB:            return MTLPixelFormatBC3_RGBA_sRGB;
        case PL_FORMAT_BC4_UNORM:           return MTLPixelFormatBC4_RUnorm;
        case PL_FORMAT_BC4_SNORM:           return MTLPixelFormatBC4_RSnorm;
        case PL_FORMAT_BC5_UNORM:           return MTLPixelFormatBC5_RGUnorm;
        case PL_FORMAT_BC5_SNORM:           return MTLPixelFormatBC5_RGSnorm;
        case PL_FORMAT_BC6H_UFLOAT:         return MTLPixelFormatBC6H_RGBUfloat;
        case PL_FORMAT_BC6H_FLOAT:          return MTLPixelFormatBC6H_RGBFloat;
        case PL_FORMAT_BC7_UNORM:           return MTLPixelFormatBC7_RGBAUnorm;
        case PL_FORMAT_BC7_SRGB:            return MTLPixelFormatBC7_RGBAUnorm_sRGB;
        case PL_FORMAT_ETC2_R8G8B8_UNORM:   return MTLPixelFormatETC2_RGB8;
        case PL_FORMAT_ETC2_R8G8B8_SRGB:    return MTLPixelFormatETC2_RGB8_sRGB;
        case PL_FORMAT_ETC2_R8G8B8A1_UNORM: return MTLPixelFormatETC2_RGB8A1;
        case PL_FORMAT_ETC2_R8G8B8A1_SRGB:  return MTLPixelFormatETC2_RGB8A1_sRGB;
        case PL_FORMAT_EAC_R11_UNORM:       return MTLPixelFormatEAC_R11Unorm;
        case PL_FORMAT_EAC_R11_SNORM:       return MTLPixelFormatEAC_R11Snorm;
        case PL_FORMAT_EAC_R11G11_UNORM:    return MTLPixelFormatEAC_RG11Unorm;
        case PL_FORMAT_EAC_R11G11_SNORM:    return MTLPixelFormatEAC_RG11Snorm;
        case PL_FORMAT_ASTC_4x4_UNORM:      return MTLPixelFormatASTC_4x4_LDR;
        case PL_FORMAT_ASTC_4x4_SRGB:       return MTLPixelFormatASTC_4x4_sRGB;
        case PL_FORMAT_ASTC_5x4_UNORM:      return MTLPixelFormatASTC_4x4_LDR;
        case PL_FORMAT_ASTC_5x4_SRGB:       return MTLPixelFormatASTC_5x4_sRGB;
        case PL_FORMAT_ASTC_5x5_UNORM:      return MTLPixelFormatASTC_5x5_LDR;
        case PL_FORMAT_ASTC_5x5_SRGB:       return MTLPixelFormatASTC_5x5_sRGB;
        case PL_FORMAT_ASTC_6x5_UNORM:      return MTLPixelFormatASTC_6x5_LDR;
        case PL_FORMAT_ASTC_6x5_SRGB:       return MTLPixelFormatASTC_6x5_sRGB;
        case PL_FORMAT_ASTC_6x6_UNORM:      return MTLPixelFormatASTC_6x6_LDR;
        case PL_FORMAT_ASTC_6x6_SRGB:       return MTLPixelFormatASTC_6x6_sRGB;
        case PL_FORMAT_ASTC_8x5_UNORM:      return MTLPixelFormatASTC_8x5_LDR;
        case PL_FORMAT_ASTC_8x5_SRGB:       return MTLPixelFormatASTC_8x5_sRGB;
        case PL_FORMAT_ASTC_8x6_UNORM:      return MTLPixelFormatASTC_8x6_LDR;
        case PL_FORMAT_ASTC_8x6_SRGB:       return MTLPixelFormatASTC_8x6_sRGB;
        case PL_FORMAT_ASTC_8x8_UNORM:      return MTLPixelFormatASTC_8x8_LDR;
        case PL_FORMAT_ASTC_8x8_SRGB:       return MTLPixelFormatASTC_8x8_sRGB;
        case PL_FORMAT_ASTC_10x5_UNORM:     return MTLPixelFormatASTC_10x5_LDR; 
        case PL_FORMAT_ASTC_10x5_SRGB:      return MTLPixelFormatASTC_10x5_sRGB;
        case PL_FORMAT_ASTC_10x6_UNORM:     return MTLPixelFormatASTC_10x6_LDR;
        case PL_FORMAT_ASTC_10x6_SRGB:      return MTLPixelFormatASTC_10x6_sRGB;
        case PL_FORMAT_ASTC_10x8_UNORM:     return MTLPixelFormatASTC_10x8_LDR;
        case PL_FORMAT_ASTC_10x8_SRGB:      return MTLPixelFormatASTC_10x8_sRGB;
        case PL_FORMAT_ASTC_10x10_UNORM:    return MTLPixelFormatASTC_10x10_LDR;
        case PL_FORMAT_ASTC_10x10_SRGB:     return MTLPixelFormatASTC_10x10_sRGB;
        case PL_FORMAT_ASTC_12x10_UNORM:    return MTLPixelFormatASTC_12x10_LDR;
        case PL_FORMAT_ASTC_12x10_SRGB:     return MTLPixelFormatASTC_12x10_sRGB;
        case PL_FORMAT_ASTC_12x12_UNORM:    return MTLPixelFormatASTC_12x12_LDR;
        case PL_FORMAT_ASTC_12x12_SRGB:     return MTLPixelFormatASTC_12x12_sRGB;
        case PL_FORMAT_S8_UINT:             return MTLPixelFormatStencil8;
    }

    PL_ASSERT(false && "Unsupported format");
    return MTLPixelFormatInvalid;
}

static MTLVertexFormat
pl__metal_vertex_format(plVertexFormat eFormat)
{
    switch(eFormat)
    {
        case PL_VERTEX_FORMAT_HALF:    return MTLVertexFormatHalf;
        case PL_VERTEX_FORMAT_HALF2:   return MTLVertexFormatHalf2;
        case PL_VERTEX_FORMAT_HALF3:   return MTLVertexFormatHalf3;
        case PL_VERTEX_FORMAT_HALF4:   return MTLVertexFormatHalf4;
        case PL_VERTEX_FORMAT_FLOAT:   return MTLVertexFormatFloat;
        case PL_VERTEX_FORMAT_FLOAT2:  return MTLVertexFormatFloat2;
        case PL_VERTEX_FORMAT_FLOAT3:  return MTLVertexFormatFloat3;
        case PL_VERTEX_FORMAT_FLOAT4:  return MTLVertexFormatFloat4;
        case PL_VERTEX_FORMAT_UCHAR:   return MTLVertexFormatUChar;
        case PL_VERTEX_FORMAT_UCHAR2:  return MTLVertexFormatUChar2;
        case PL_VERTEX_FORMAT_UCHAR3:  return MTLVertexFormatUChar3;
        case PL_VERTEX_FORMAT_UCHAR4:  return MTLVertexFormatUChar4;
        case PL_VERTEX_FORMAT_CHAR:    return MTLVertexFormatChar;
        case PL_VERTEX_FORMAT_CHAR2:   return MTLVertexFormatChar2;
        case PL_VERTEX_FORMAT_CHAR3:   return MTLVertexFormatChar3;
        case PL_VERTEX_FORMAT_CHAR4:   return MTLVertexFormatChar4;
        case PL_VERTEX_FORMAT_USHORT:  return MTLVertexFormatUShort;
        case PL_VERTEX_FORMAT_USHORT2: return MTLVertexFormatUShort2;
        case PL_VERTEX_FORMAT_USHORT3: return MTLVertexFormatUShort3;
        case PL_VERTEX_FORMAT_USHORT4: return MTLVertexFormatUShort4;
        case PL_VERTEX_FORMAT_SHORT:   return MTLVertexFormatShort;
        case PL_VERTEX_FORMAT_SHORT2:  return MTLVertexFormatShort2;
        case PL_VERTEX_FORMAT_SHORT3:  return MTLVertexFormatShort3;
        case PL_VERTEX_FORMAT_SHORT4:  return MTLVertexFormatShort4;
        case PL_VERTEX_FORMAT_UINT:    return MTLVertexFormatUInt;
        case PL_VERTEX_FORMAT_UINT2:   return MTLVertexFormatUInt2;
        case PL_VERTEX_FORMAT_UINT3:   return MTLVertexFormatUInt3;
        case PL_VERTEX_FORMAT_UINT4:   return MTLVertexFormatUInt4;
        case PL_VERTEX_FORMAT_INT:     return MTLVertexFormatInt;
        case PL_VERTEX_FORMAT_INT2:    return MTLVertexFormatInt2;
        case PL_VERTEX_FORMAT_INT3:    return MTLVertexFormatInt3;
        case PL_VERTEX_FORMAT_INT4:    return MTLVertexFormatInt4;
        
        case PL_VERTEX_FORMAT_DOUBLE:
        case PL_VERTEX_FORMAT_DOUBLE2:
        case PL_VERTEX_FORMAT_DOUBLE3:
        case PL_VERTEX_FORMAT_DOUBLE4:
        {
            PL_ASSERT(false && "Unsupported vertex format");
            return MTLVertexFormatInvalid;
        }
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
pl__metal_stage_flags(plShaderStageFlags tFlags)
{
    MTLRenderStages tResult = 0;

    if(tFlags & PL_SHADER_STAGE_VERTEX)   tResult |= MTLRenderStageVertex;
    if(tFlags & PL_SHADER_STAGE_FRAGMENT)    tResult |= MTLRenderStageFragment;
    // if(tFlags & PL_SHADER_STAGE_COMPUTE)  tResult |= VK_SHADER_STAGE_COMPUTE_BIT; // not needed

    return tResult;
}

static void
pl__garbage_collect(plDevice* ptDevice)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtShaders); i++)
    {
        const uint16_t iResourceIndex = ptGarbage->sbtShaders[i].uIndex;
        plShader* ptResource = pl_graphics_get_shader(ptDevice, ptGarbage->sbtShaders[i]);

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
        plBindGroup* ptResource = &ptDevice->sbtBindGroupsCold[iBindGroupIndex];
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

    uint64_t uSignaledValue = ptFrame->tFrameBoundaryEvent.signaledValue;
    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtTextures); i++)
    {
        const uint16_t uTextureIndex = ptGarbage->sbtTextures[i].uIndex;
        if( uSignaledValue > ptDevice->sbtTexturesCold[uTextureIndex]._uFrameBoundaryValueForDeletion + 1 && !ptDevice->sbtTexturesHot[uTextureIndex].bSwapchain)
        {
            plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[uTextureIndex];
            [ptMetalTexture->tTexture release];
            ptMetalTexture->tTexture = nil;
            pl_sb_push(ptDevice->sbtTextureFreeIndices, uTextureIndex);
            pl_sb_del(ptGarbage->sbtTextures, i);
            i--;
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtBuffers); i++)
    {
        const uint16_t iBufferIndex = ptGarbage->sbtBuffers[i].uIndex;
        if( uSignaledValue > ptDevice->sbtBuffersCold[iBufferIndex]._uFrameBoundaryValueForDeletion + 1)
        {
            [ptDevice->sbtBuffersHot[iBufferIndex].tBuffer release];
            ptDevice->sbtBuffersHot[iBufferIndex].tBuffer = nil;
            pl_sb_push(ptDevice->sbtBufferFreeIndices, iBufferIndex);
            PL_LOG_DEBUG_API_F(gptLog, uLogChannelGraphics, "Delete buffer %u for deletion frame %llu", iBufferIndex, gptIO->ulFrameCount);
            pl_sb_del(ptGarbage->sbtBuffers, i);
            i--;
        }
    }

    for(uint32_t i = 0; i < pl_sb_size(ptGarbage->sbtMemory); i++)
    {
        if( uSignaledValue > ptGarbage->sbtMemory[i]._uFrameBoundaryValueForDeletion + 1)
        {
            if(ptGarbage->sbtMemory[i].ptAllocator)
            {
                ptGarbage->sbtMemory[i].ptAllocator->free(ptGarbage->sbtMemory[i].ptAllocator->ptInst, &ptGarbage->sbtMemory[i]);
            }
            else
            {
                pl_graphics_free_memory(ptDevice, &ptGarbage->sbtMemory[i]);
            }
            pl_sb_del(ptGarbage->sbtMemory, i);
            i--;
        }
    }

    // pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtShaders);
    pl_sb_reset(ptGarbage->sbtComputeShaders);
    pl_sb_reset(ptGarbage->sbtRenderPasses);
    pl_sb_reset(ptGarbage->sbtRenderPassLayouts);
    // pl_sb_reset(ptGarbage->sbtMemory);
    // pl_sb_reset(ptGarbage->sbtBuffers);
    pl_sb_reset(ptGarbage->sbtBindGroups);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

//-----------------------------------------------------------------------------
// [SECTION] device memory allocators
//-----------------------------------------------------------------------------

plDeviceMemoryAllocation
pl_graphics_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryFlags tMemoryFlags, uint32_t uTypeFilter, const char* pcName)
{
    if(pcName == NULL)
    {
        pcName = "unnamed memory block";
    }

    plDeviceMemoryAllocation tBlock = {
        .uHandle = UINT64_MAX,
        .ulSize  = (uint64_t)szSize,
        .tMemoryFlags = tMemoryFlags
    };

    MTLHeapDescriptor* ptHeapDescriptor = [MTLHeapDescriptor new];
    ptHeapDescriptor.size = tBlock.ulSize;
    ptHeapDescriptor.type = MTLHeapTypePlacement;
    ptHeapDescriptor.hazardTrackingMode = MTLHazardTrackingModeUntracked;

    if(tMemoryFlags & PL_MEMORY_FLAGS_DEVICE_LOCAL)
    {
        ptHeapDescriptor.storageMode = MTLStorageModePrivate;
        gptGraphics->szLocalMemoryInUse += tBlock.ulSize;
    }
    else
    {
        ptHeapDescriptor.storageMode = MTLStorageModeShared;
        gptGraphics->szHostMemoryInUse += tBlock.ulSize;
    }

    id<MTLHeap> tNewHeap = [ptDevice->tDevice newHeapWithDescriptor:ptHeapDescriptor];
    tNewHeap.label = [NSString stringWithUTF8String:pcName];
    // tBlock.uHandle = (uint64_t)tNewHeap;

    if(pl_sb_size(ptDevice->sbuFreeHeaps) > 0)
    {
        uint64_t uFreeIndex = pl_sb_pop(ptDevice->sbuFreeHeaps);
        ptDevice->atHeaps[uFreeIndex] = tNewHeap;
        ptDevice->apcHeapNames[uFreeIndex] = pcName;
        tBlock.uHandle = uFreeIndex;
    }
    else
    {
        PL_ASSERT(false && "only 64 allocations allowed");
    }

    [ptHeapDescriptor release];
    strncpy(tBlock.acName, pcName, 64);
    pl_sb_push(ptDevice->sbtMemoryBlocks, tBlock);

    [ptDevice->tResidencySet addAllocation:tNewHeap];
    [ptDevice->tResidencySet commit];
    [ptDevice->tResidencySet requestResidency];
    return tBlock;
}

void
pl_graphics_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    const uint32_t uMemoryBlockCount = pl_sb_size(ptDevice->sbtMemoryBlocks);
    for(uint32_t i = 0; i < uMemoryBlockCount; i++)
    {
        if(ptDevice->sbtMemoryBlocks[i].uHandle == ptBlock->uHandle)
        {
            pl_sb_del_swap(ptDevice->sbtMemoryBlocks, i);
            break;
        }
    }

    id<MTLHeap> tHeap = ptDevice->atHeaps[ptBlock->uHandle];

    [ptDevice->tResidencySet removeAllocation:tHeap];
    [ptDevice->tResidencySet commit];

    pl_sb_push(ptDevice->sbuFreeHeaps, ptBlock->uHandle);

    [tHeap setPurgeableState:MTLPurgeableStateEmpty];
    [tHeap release];
    tHeap = nil;
    ptDevice->atHeaps[ptBlock->uHandle] = nil;

    if(ptBlock->tMemoryFlags & PL_MEMORY_FLAGS_DEVICE_LOCAL)
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
    ptBlock->tMemoryFlags = 0;
    ptBlock->ulMemoryType = 0;
}

bool
pl_graphics_flush_memory(plDevice* ptDevice, uint32_t uRangeCount, const plDeviceMemoryRange* atRanges)
{
    return true;
}

bool
pl_graphics_invalidate_memory(plDevice* ptDevice, uint32_t uRangeCount, const plDeviceMemoryRange* atRanges)
{
    return true;
}

void
pl_graphics_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{

    ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBufferFreeIndices, tHandle.uIndex);

    [ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer release];
    ptDevice->sbtBuffersHot[tHandle.uIndex].tBuffer = nil;

    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    if(ptBuffer->tMemoryAllocation.ptAllocator)
        ptBuffer->tMemoryAllocation.ptAllocator->free(ptBuffer->tMemoryAllocation.ptAllocator->ptInst, &ptBuffer->tMemoryAllocation);
    else
        pl_graphics_free_memory(ptDevice, &ptBuffer->tMemoryAllocation);
}

void
pl_graphics_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    pl_sb_push(ptDevice->sbtTextureFreeIndices, tHandle.uIndex);
    ptDevice->sbtTexturesCold[tHandle.uIndex]._uGeneration++;

    PL_LOG_DEBUG_API_F(gptLog, uLogChannelGraphics, "destroy texture %s immediately (%u)", ptDevice->sbtTexturesCold[tHandle.uIndex].tDesc.pcDebugName, tHandle.uIndex);

    plMetalTexture* ptMetalTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];
    [ptMetalTexture->tTexture release];
    ptMetalTexture->tTexture = nil;

    plTexture* ptTexture = &ptDevice->sbtTexturesCold[tHandle.uIndex];
    if(ptTexture->tMemoryAllocation.ptAllocator)
        ptTexture->tMemoryAllocation.ptAllocator->free(ptTexture->tMemoryAllocation.ptAllocator->ptInst, &ptTexture->tMemoryAllocation);
    else
        pl_graphics_free_memory(ptDevice, &ptTexture->tMemoryAllocation);
}

void
pl_graphics_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    ptDevice->sbtBindGroupsCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBindGroupFreeIndices, tHandle.uIndex);

    plMetalBindGroup* ptMetalResource = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    plBindGroup* ptResource = &ptDevice->sbtBindGroupsCold[tHandle.uIndex];
    [ptMetalResource->tShaderArgumentBuffer release];
    ptMetalResource->tShaderArgumentBuffer = nil;
}

void
pl_graphics_destroy_bind_group_layout(plDevice* ptDevice, plBindGroupLayoutHandle tHandle)
{
    ptDevice->sbtBindGroupLayoutsCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBindGroupLayoutFreeIndices, tHandle.uIndex);
}

void
pl_graphics_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
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

void
pl_graphics_destroy_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
    PL_LOG_TRACE_API_F(gptLog, uLogChannelGraphics, "destroy sampler %u immediately", tHandle.uIndex);
    [ptDevice->sbtSamplersHot[tHandle.uIndex].tSampler release];
    ptDevice->sbtSamplersHot[tHandle.uIndex].tSampler = nil;
    ptDevice->sbtSamplersCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtSamplerFreeIndices, tHandle.uIndex);
}

void
pl_graphics_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    ptDevice->sbtComputeShadersCold[tHandle.uIndex]._uGeneration++;

    plComputeShader* ptResource = &ptDevice->sbtComputeShadersCold[tHandle.uIndex];
    plMetalComputeShader* ptVariantMetalResource = &ptDevice->sbtComputeShadersHot[tHandle.uIndex];
    [ptVariantMetalResource->tPipelineState release];
    ptVariantMetalResource->tPipelineState = nil;
    pl_sb_push(ptDevice->sbtComputeShaderFreeIndices, tHandle.uIndex);
}

id<MTLDevice>
pl_graphics_get_metal_device(plDevice* ptDevice)
{
    return ptDevice->tDevice;
}

id<MTL4CommandBuffer>
pl_graphics_get_metal_command_buffer(plCommandBuffer* ptCommandBuffer)
{
    return ptCommandBuffer->tCmdBuffer4;
}

id<MTL4RenderCommandEncoder>
pl_graphics_get_metal_command_encoder(plCommandBuffer* ptCmdBuffer)
{
    return ptCmdBuffer->ptDevice->tRenderEncoder;
}

id<MTLTexture>
pl_graphics_get_metal_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    return ptDevice->sbtTexturesHot[tHandle.uIndex].tTexture;
}

plTextureHandle
pl_graphics_get_metal_bind_group_texture(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    return ptDevice->sbtBindGroupsHot[tHandle.uIndex].tFirstTexture;
}
