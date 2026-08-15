/*
   pl_graphics_cpu.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal structs
// [SECTION] opaque structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] drawing
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_string.h"
#include "pl_memory.h"
#include "pl_graphics_internal.h"
#include "pl_shader_interop_cpu.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef int plCpuCommandBufferItemType;

enum _plCpuCommandBufferItemType
{
    PL_CPU_COMMAND_BUFFER_ITEM_TYPE_NONE = 0,
    PL_CPU_COMMAND_BUFFER_ITEM_TYPE_DRAW_INDEXED,
    PL_CPU_COMMAND_BUFFER_ITEM_TYPE_COPY_BUFFER_TO_TEXTURE,
    PL_CPU_COMMAND_BUFFER_ITEM_TYPE_SET_VIEWPORT,
    PL_CPU_COMMAND_BUFFER_ITEM_TYPE_SET_SCISSOR,
};

typedef struct _plInternalDeviceAllocatorData
{
    plDevice*                 ptDevice;
    plDeviceMemoryAllocatorI* ptAllocator;
} plInternalDeviceAllocatorData;

typedef struct _plCpuDynamicBuffer
{
    uint32_t                 uHandle;
    plDeviceMemoryAllocation tMemory;
} plCpuDynamicBuffer;

typedef struct _plCpuBuffer
{
    void* pData;
} plCpuBuffer;

typedef struct _plCpuTexture
{
    // VkImage     tImage;
    // VkImageView tImageView;
    void*       pData;
    bool        bOriginalView; // so we only free if original
} plCpuTexture;

typedef struct _plCpuSampler
{
    int a;
} plCpuSampler;

typedef struct _plCpuBindGroupLayout
{
    int a;
} plCpuBindGroupLayout;

typedef struct _plCpuBindGroup
{
    bool bResetable;
    plDescriptor* atDescriptors;
    plBindGroupPool* ptPool;
} plCpuBindGroup;

typedef struct _plCpuShader
{
    plVertexShader tVertexShader;
    plPixelShader  tPixelShader;
    size_t         szSpecializationSize;
} plCpuShader;

typedef struct _plCpuComputeShader
{
    // VkPipelineLayout         tPipelineLayout;
    // VkPipeline               tPipeline;
    // VkShaderModule           tShaderModule;
    // VkSpecializationMapEntry atSpecializationEntries[PL_MAX_SHADER_SPECIALIZATION_CONSTANTS];
    size_t                   szSpecializationSize;
    // VkDescriptorSetLayout    atDescriptorSetLayouts[4];
} plCpuComputeShader;

//-----------------------------------------------------------------------------
// [SECTION] opaque structs
//-----------------------------------------------------------------------------

typedef struct _plCommandBufferItem
{
    plCpuCommandBufferItemType eType;

    // draw
    plRenderInfo    tRenderInfo;
    plDrawIndex     tDraw;
    plShaderHandle  tShader;
    plBufferHandle  tVertexBuffer;
    plDescriptorSet atCurrentDescriptorSets[4];

    // copy buffer to texture
    plBufferImageCopy tBufferImageCopy;
    plBufferHandle tBufferHandle;
    plTextureHandle tTextureHandle;

    // set viewport
    plRenderViewport tViewport;

    // set scissor
    plScissor tScissor;
} plCommandBufferItem;

typedef struct _plVertexCache
{
    bool          bValid;
    plVec2        tPosition;
    plVaryingData tVaryings;
} plVertexCache;

typedef struct _plCommandBuffer
{
    plDevice*          ptDevice; // for convience
    plCommandPool*     ptPool; // parent pool
    plCommandBuffer*   ptNext; // for linked list

    plRenderInfo         tCurrentRenderInfo;
    plShaderHandle       tCurrentShader;
    plBufferHandle       tCurrentVertexBuffer;
    plCommandBufferItem* sbtStream;
    uint32_t             uCurrentStreamItem;
    plDescriptorSet      atCurrentDescriptorSets[4];

    plVertexCache* sbtVertexCache;
    
} plCommandBuffer;

typedef struct _plCommandPool
{
    plDevice*        ptDevice; // for convience
    plCommandBuffer* ptCommandBufferFreeList;  // free list of command buffers
} plCommandPool;

typedef struct _plBindGroupPool
{
    plDevice*           ptDevice; // for convience
    plBindGroupPoolDesc tDesc;
} plBindGroupPool;

typedef struct _plTimelineSemaphore
{
    plDevice*            ptDevice; // for convience
    plTimelineSemaphore* ptNext; // for linked list
} plTimelineSemaphore;

typedef struct _plFrameContext
{
    // VkSemaphore    tRenderFinish;
    // VkFence        tInFlight;
    // VkFramebuffer* sbtRawFrameBuffers;

    // dynamic buffer stuff
    uint16_t               uCurrentBufferIndex;
    uint64_t               uNextValue;
    plCpuDynamicBuffer* sbtDynamicBuffers;
} plFrameContext;

typedef struct _plDevice
{
    // general
    plDeviceInit              tInit;
    plDeviceInfo              tInfo;
    plFrameGarbage*           sbtGarbage;
    plFrameContext*           sbtFrames;
    plDeviceMemoryAllocatorI* ptDynamicAllocator;
    
    // timeline semaphore free list
    plTimelineSemaphore* ptSemaphoreFreeList;

    // shader generation pool
    plCpuShader* sbtShadersHot;
    plShader*    sbtShadersCold;
    uint16_t*    sbtShaderFreeIndices;

    // compute shader generation pool
    plCpuComputeShader* sbtComputeShadersHot;
    plComputeShader*    sbtComputeShadersCold;
    uint16_t*           sbtComputeShaderFreeIndices;

    // buffer generation pool
    plCpuBuffer* sbtBuffersHot;
    plBuffer*    sbtBuffersCold;
    uint16_t*    sbtBufferFreeIndices;

    // texture generation pool
    // VkImageView*     sbtTextureViewsHot;
    plCpuTexture* sbtTexturesHot;
    plTexture*    sbtTexturesCold;
    uint16_t*     sbtTextureFreeIndices;

    // sampler generation pool
    int*       sbtSamplersHot;
    plSampler* sbtSamplersCold;
    uint16_t*  sbtSamplerFreeIndices;

    // bind group generation pool
    plCpuBindGroup* sbtBindGroupsHot;
    plBindGroup*    sbtBindGroupsCold;
    uint16_t*       sbtBindGroupFreeIndices;

    // bind group layout generation pool
    plCpuBindGroupLayout* sbtBindGroupLayoutsHot;
    plBindGroupLayout*    sbtBindGroupLayoutsCold;
    uint16_t*             sbtBindGroupLayoutFreeIndices;

    // memory blocks
    plDeviceMemoryAllocation* sbtMemoryBlocks;

    plStackedBarrier* sbtBarrierStack;
} plDevice;

typedef struct _plGraphics
{
    // general
    uint32_t uCurrentFrameIndex;
    uint32_t uFramesInFlight;
    size_t   szLocalMemoryInUse;
    size_t   szHostMemoryInUse;
    bool     bValidationActive;
    bool     bDebugMessengerActive;

    // free lists
    plCommandBuffer*  ptCommandBufferFreeList;
    
    // specifics
    plTempAllocator tTempAllocator;
} plGraphics;

typedef struct _plSurface
{
    plWindowSurface* ptWindowSurface;
} plSurface;

typedef struct _plSwapchain
{
    plDevice*        ptDevice; // for convience
    plSwapchainInfo  tInfo;
    uint32_t         uImageCount;
    plTextureHandle* sbtSwapchainTextureViews;
    uint32_t         uCurrentImageIndex; // current image to use within the swap chain

    // platform specific
    plSurface* ptSurface;
    plWindowSurfaceImage tCurrentSurfaceImage;
} plSwapchain;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline float
pl__edge_function(plVec2 one, plVec2 two, plVec2 three)
{
    return(two.x - one.x) * (three.y - one.y) - (two.y - one.y) * (three.x - one.x);
};

static inline int64_t
pl__edge_function_fixed(int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t px, int64_t py)
{
    return (ay - by) * px + (bx - ax) * py + ax * by - bx * ay;
}

static inline bool
pl__is_top_left_edge_fixed(int64_t ax, int64_t ay, int64_t bx, int64_t by)
{
    const int64_t dx = bx - ax;
    const int64_t dy = by - ay;
    return dy < 0 || (dy == 0 && dx > 0);
}

// 2-value minimum 

// 3-value minimum 
static inline int pl_min3(int a, int b, int c) {
    int temp = (a < b) ? a : b;
    return (temp < c) ? temp : c; 
}


// 3-value maximum 
static inline int pl_max3(int a, int b, int c) {
    int temp = (a > b) ? a : b;
    return (temp > c) ? temp : c;
}

static inline bool
pl__is_top_left_edge(plVec2 a, plVec2 b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;

    return dy < 0.0f || (dy == 0.0f && dx > 0.0f);
}

static void
pl_set_pixel(plCpuTexture* ptTexture, plVec2 input, plVec4 tColor, uint32_t uWidth, uint32_t uHeight)
{
    if(input.x < 0)
        return;
    if(input.y < 0)
        return;
    if(input.x >= uWidth)
        return;
    if(input.y >= uHeight)
        return;

    int iRowOffset = uWidth * 4 * (int)input.y;
    int iPixelStart = iRowOffset + (int)input.x * 4;
    uint8_t* puData = ptTexture->pData;

    const float srcA = tColor.a;
    const float invA = 1.0f - srcA;

    plVec4 tDestColor;
    tDestColor.r = puData[iPixelStart + 0];
    tDestColor.g = puData[iPixelStart + 1];
    tDestColor.b = puData[iPixelStart + 2];
    tDestColor.a = puData[iPixelStart + 3];

    puData[iPixelStart + 0] = (unsigned char)((255.0f * tColor.r * srcA + tDestColor.r * invA));
    puData[iPixelStart + 1] = (unsigned char)((255.0f * tColor.g * srcA + tDestColor.g * invA));
    puData[iPixelStart + 2] = (unsigned char)((255.0f * tColor.b * srcA + tDestColor.b * invA));
    puData[iPixelStart + 3] = (unsigned char)((255.0f * tColor.a + tDestColor.a * invA));
};

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plTimelineSemaphore*
pl_graphics_create_semaphore(plDevice* ptDevice, bool bHostVisible)
{
    plTimelineSemaphore* ptSemaphore = pl__get_new_semaphore(ptDevice);
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
}

void
pl_graphics_wait_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore, uint64_t ulValue)
{
}

uint64_t
pl_graphics_get_semaphore_value(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore)
{
    uint64_t ulValue = 0;
    return ulValue;
}

plBufferHandle
pl_graphics_create_buffer(plDevice* ptDevice, const plBufferDesc* ptDesc, plBuffer **ptBufferOut)
{
    plBufferHandle tHandle = pl__get_new_buffer_handle(ptDevice);
    plBuffer* ptBuffer = pl_graphics_get_buffer(ptDevice, tHandle);
    ptBuffer->tDesc = *ptDesc;

    if(ptDesc->pcDebugName == NULL)
    {
        ptBuffer->tDesc.pcDebugName = "unnamed buffer";
    }

    plCpuBuffer tCPUBuffer = {0};

    ptBuffer->tMemoryRequirements.ulSize = ptDesc->szByteSize;
    ptBuffer->tMemoryRequirements.ulAlignment = 0;
    ptBuffer->tMemoryRequirements.uMemoryTypeBits = 0;

    ptDevice->sbtBuffersHot[tHandle.uIndex] = tCPUBuffer;

    if(ptBufferOut)
    {
        *ptBufferOut = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    }

    return tHandle;
}

void
pl_graphics_bind_buffer_to_memory(plDevice* ptDevice, plBufferHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{   
    plBuffer* ptBuffer = &ptDevice->sbtBuffersCold[tHandle.uIndex];
    ptBuffer->tMemoryAllocation = *ptAllocation;
    plCpuBuffer* ptCpuBuffer = &ptDevice->sbtBuffersHot[tHandle.uIndex];
    ptCpuBuffer->pData = &((uint8_t*)ptAllocation->uHandle)[ptAllocation->ulOffset];

}

void
pl_graphics_reset_dynamic_data_blocks(plDevice* ptDevice)
{
}

plDynamicDataBlock
pl_graphics_allocate_dynamic_data_block(plDevice* ptDevice)
{
    plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);


    plCpuDynamicBuffer* ptDynamicBuffer = NULL;
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
            gptGraphics->szHostMemoryInUse += ptDevice->tInit.szDynamicBufferBlockSize;
        }
    }
    
    if(ptDynamicBuffer == NULL)
        ptDynamicBuffer = &ptFrame->sbtDynamicBuffers[ptFrame->uCurrentBufferIndex];

    plDynamicDataBlock tBlock = {
        ._uBufferHandle  = ptFrame->uCurrentBufferIndex,
        ._uByteSize      = (uint32_t)ptDevice->tInit.szDynamicBufferBlockSize,
        ._pcData         = ptDynamicBuffer->tMemory.pHostMapped,
        ._uAlignment     = 256,
        ._uBumpAmount    = (uint32_t)ptDevice->tInit.szDynamicDataMaxSize,
        ._uCurrentOffset = 0
    };
    tBlock._uBumpAmount = pl_min(tBlock._uAlignment, tBlock._uBumpAmount);
    if(uDynamicBufferCount > 0)
        ptFrame->uCurrentBufferIndex++;
    return tBlock;
}

void
pl_graphics_copy_texture_to_buffer(plCommandBuffer* ptEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    // PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_copy_texture(plCommandBuffer* ptEncoder, plTextureHandle tSrcHandle, plTextureHandle tDstHandle, uint32_t uRegionCount, const plImageCopy* ptRegions)
{
    // PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_generate_mipmaps(plCommandBuffer* ptEncoder, plTextureHandle tTexture)
{
    // PL_ASSERT(false && "NOT IMPLEMENTED");
}

plSamplerHandle
pl_graphics_create_sampler(plDevice* ptDevice, const plSamplerDesc* ptDesc)
{
    plSamplerHandle tHandle = pl__get_new_sampler_handle(ptDevice);
    return tHandle;
}

plBindGroupHandle
pl_graphics_create_bind_group(plDevice* ptDevice, const plBindGroupDesc* ptDesc)
{
    plBindGroupHandle tHandle = pl__get_new_bind_group_handle(ptDevice);
    plBindGroup* ptBindGroup = pl_graphics_get_bind_group(ptDevice, tHandle);
    ptBindGroup->tDesc = *ptDesc;
    plBindGroupLayout* ptBindGroupLayout = pl_graphics_get_bind_group_layout(ptDevice, ptDesc->tLayout);
  
    plCpuBindGroup tCpuBindGroup = {0};
    tCpuBindGroup.ptPool = ptDesc->ptPool;
    tCpuBindGroup.atDescriptors = PL_ALLOC(sizeof(plDescriptor) * ptBindGroupLayout->_uDescriptorCount);
    memset(tCpuBindGroup.atDescriptors, 0, sizeof(plDescriptor) * ptBindGroupLayout->_uDescriptorCount);
    ptDevice->sbtBindGroupsHot[tHandle.uIndex] = tCpuBindGroup;
    return tHandle;
}

plBindGroupLayoutHandle
pl_graphics_create_bind_group_layout(plDevice* ptDevice, const plBindGroupLayoutDesc* ptDesc)
{
    plBindGroupLayoutHandle tHandle = pl__get_new_bind_group_layout_handle(ptDevice);
    plBindGroupLayout* ptLayout = &ptDevice->sbtBindGroupLayoutsCold[tHandle.uIndex];

    // count bindings
    ptLayout->tDesc = *ptDesc;
    ptLayout->_uBufferBindingCount = 0;
    ptLayout->_uTextureBindingCount = 0;
    ptLayout->_uSamplerBindingCount = 0;
    ptLayout->_uDescriptorCount = 0;
    for(uint32_t i = 0; i < PL_MAX_TEXTURES_PER_BIND_GROUP; i++)
    {
        if(ptDesc->atTextureBindings[i].eStages == PL_SHADER_STAGE_NONE)
            break;
        ptLayout->_uTextureBindingCount++;
        if(ptDesc->atTextureBindings[i].uDescriptorCount > 1)
            ptLayout->_uDescriptorCount += ptDesc->atTextureBindings[i].uDescriptorCount;
        else
            ptLayout->_uDescriptorCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_BUFFERS_PER_BIND_GROUP; i++)
    {
        if(ptDesc->atBufferBindings[i].eStages == PL_SHADER_STAGE_NONE)
            break;
        ptLayout->_uBufferBindingCount++;
         ptLayout->_uDescriptorCount++;
    }

    for(uint32_t i = 0; i < PL_MAX_SAMPLERS_PER_BIND_GROUP; i++)
    {
        if(ptDesc->atSamplerBindings[i].eStages == PL_SHADER_STAGE_NONE)
            break;
        ptLayout->_uSamplerBindingCount++;
         ptLayout->_uDescriptorCount++;
    }

    plCpuBindGroupLayout tCpuLayout = {0};
    ptDevice->sbtBindGroupLayoutsHot[tHandle.uIndex] = tCpuLayout;

    return tHandle;
}

void
pl_graphics_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
    plCpuBindGroup* ptCpuBindGroup = &ptDevice->sbtBindGroupsHot[tHandle.uIndex];
    plBindGroup* ptBindGroup = pl_graphics_get_bind_group(ptDevice, tHandle);

    plBindGroupLayout* ptBindGroupLayout = pl_graphics_get_bind_group_layout(ptDevice, ptBindGroup->tDesc.tLayout);

    for(uint32_t i = 0; i < ptBindGroupLayout->_uBufferBindingCount; i++)
    {
        const plBindGroupUpdateBufferData* ptUpdate = &ptData->atBufferBindings[i];
        ptCpuBindGroup->atDescriptors[ptUpdate->uSlot].eType = PL_DESCRIPTOR_TYPE_BUFFER;
    }

    for(uint32_t i = 0; i < ptBindGroupLayout->_uTextureBindingCount; i++)
    {
        const plBindGroupUpdateTextureData* ptUpdate = &ptData->atTextureBindings[i];
        plTexture* ptTexture = pl_graphics_get_texture(ptDevice, ptUpdate->tTexture);
        plCpuTexture* ptCpuTexture = &ptDevice->sbtTexturesHot[ptUpdate->tTexture.uIndex];
        ptCpuBindGroup->atDescriptors[ptUpdate->uSlot].eType = PL_DESCRIPTOR_TYPE_TEXTURE;
        ptCpuBindGroup->atDescriptors[ptUpdate->uSlot].uWidth = (uint32_t)ptTexture->tDesc.tDimensions.x;
        ptCpuBindGroup->atDescriptors[ptUpdate->uSlot].uHeight = (uint32_t)ptTexture->tDesc.tDimensions.y;
        ptCpuBindGroup->atDescriptors[ptUpdate->uSlot].uComponents = 4;
        ptCpuBindGroup->atDescriptors[ptUpdate->uSlot].puData = ptCpuTexture->pData;
    }

    for(uint32_t i = 0; i < ptBindGroupLayout->_uSamplerBindingCount; i++)
    {
        const plBindGroupUpdateSamplerData* ptUpdate = &ptData->atSamplerBindings[i];
        ptCpuBindGroup->atDescriptors[ptUpdate->uSlot].eType = PL_DESCRIPTOR_TYPE_SAMPLER;
    }
}

plTextureHandle
pl_graphics_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, plTexture **ptTextureOut)
{
    plTextureDesc tDesc = *ptDesc;

    if (tDesc.pcDebugName == NULL)
        tDesc.pcDebugName = "unnamed texture";

    if (tDesc.uMips == 0)
        tDesc.uMips = (uint32_t)floorf(log2f((float)pl_maxi((int)tDesc.tDimensions.x, (int)tDesc.tDimensions.y))) + 1u;

    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    plTexture* ptTexture = pl_graphics_get_texture(ptDevice, tHandle);
    ptTexture->tDesc = tDesc;
    ptTexture->tView = (plTextureViewDesc){
        .eFormat     = tDesc.eFormat,
        .uBaseMip    = 0,
        .uMips       = tDesc.uMips,
        .uBaseLayer  = 0,
        .uLayerCount = tDesc.uLayers,
        .tTexture    = tHandle
    };

    // get memory requirements
    plCpuTexture tCpuTexture = {0};
    ptTexture->tMemoryRequirements.ulSize = (uint32_t)(tDesc.tDimensions.x * tDesc.tDimensions.y * tDesc.tDimensions.z) * pl__format_stride(tDesc.eFormat);
    ptTexture->tMemoryRequirements.ulAlignment = 0;
    ptTexture->tMemoryRequirements.uMemoryTypeBits = 0;

    ptDevice->sbtTexturesHot[tHandle.uIndex] = tCpuTexture;
    if (ptTextureOut)
       *ptTextureOut = &ptDevice->sbtTexturesCold[tHandle.uIndex];

    return tHandle;
}

void
pl_graphics_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
    plTexture* ptTexture = pl_graphics_get_texture(ptDevice, tHandle);
    ptTexture->tMemoryAllocation = *ptAllocation;
    plCpuTexture* ptCpuTexture = &ptDevice->sbtTexturesHot[tHandle.uIndex];
    ptCpuTexture->pData = &((uint8_t*)ptAllocation->uHandle)[ptAllocation->ulOffset];
}

plTextureHandle
pl_graphics_create_texture_view(plDevice* ptDevice, const plTextureViewDesc* ptViewDesc)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    return tHandle;
}

plComputeShaderHandle
pl_graphics_create_compute_shader(plDevice* ptDevice, const plComputeShaderDesc* ptDescription)
{
    plComputeShaderHandle tHandle = pl__get_new_compute_shader_handle(ptDevice);
    return tHandle;
}

plShaderHandle
pl_graphics_create_shader(plDevice* ptDevice, const plShaderDesc* ptDescription)
{
    plShaderHandle tHandle = pl__get_new_shader_handle(ptDevice);
    plShader* ptShader = pl_graphics_get_shader(ptDevice, tHandle);
    ptShader->tDesc = *ptDescription;

    plCpuShader* ptCpuShader = &ptDevice->sbtShadersHot[tHandle.uIndex];
    ptCpuShader->tVertexShader = (plVertexShader)ptDescription->tVertexShader.puCode;
    ptCpuShader->tPixelShader = (plPixelShader)ptDescription->tFragmentShader.puCode;

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
            if (ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].eFormat == PL_VERTEX_FORMAT_UNKNOWN)
                break;
            ptShader->tDesc.atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uLocation = abExplicitAttributePosition[uVtxBufferIdx] ? ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uLocation : uCurrentAttributeCount;
            ptShader->tDesc.atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uByteOffset = abExplicitOffset[uVtxBufferIdx] ? ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].uByteOffset : (uint32_t)uOffset;
            uOffset += pl__get_vertex_attribute_size(ptDescription->atVertexBufferLayouts[uVtxBufferIdx].atAttributes[i].eFormat);
            uCurrentAttributeCount++;
        }
        ptShader->tDesc.atVertexBufferLayouts[uVtxBufferIdx].uByteStride = abExplicitStride[uVtxBufferIdx] ? ptDescription->atVertexBufferLayouts[uVtxBufferIdx].uByteStride : (uint32_t)auCalculatedStrides[uVtxBufferIdx];
    }
    return tHandle;
}

plTextureHandle*
pl_graphics_get_swapchain_images(plSwapchain* ptSwap, uint32_t* puSizeOut)
{
    *puSizeOut = ptSwap->uImageCount;
    return ptSwap->sbtSwapchainTextureViews;
}

void
pl_graphics_begin_command_recording(plCommandBuffer* ptCommandBuffer)
{
    pl_sb_reset(ptCommandBuffer->sbtStream);
    ptCommandBuffer->uCurrentStreamItem = UINT32_MAX;
}

void
pl_graphics_begin_render_pass(plCommandBuffer* ptCmdBuffer, const plRenderInfo* ptInfo, const plPassResources* ptResource)
{
    ptCmdBuffer->tCurrentRenderInfo = *ptInfo;

    plScissor tScissor = {
        .iOffsetX = 0,
        .iOffsetY = 0,
        .uWidth  = (uint32_t)ptInfo->tRenderArea.tMax.x,
        .uHeight = (uint32_t)ptInfo->tRenderArea.tMax.y
    };
    pl_graphics_set_scissor_region(ptCmdBuffer, &tScissor);

    plRenderViewport tViewport = {
        .fWidth    = ptInfo->tRenderArea.tMax.x,
        .fHeight   = ptInfo->tRenderArea.tMax.y,
        .fMaxDepth    = 1.0f
    };
    pl_graphics_set_viewport(ptCmdBuffer, &tViewport);

    plDevice* ptDevice = ptCmdBuffer->ptDevice;
    plTexture* ptTexture = &ptDevice->sbtTexturesCold[ptInfo->atColorAttachments[0].tTexture.uIndex];
    plCpuTexture* ptCpuTexture = &ptDevice->sbtTexturesHot[ptInfo->atColorAttachments[0].tTexture.uIndex];
    memset(ptCpuTexture->pData, 0, (uint32_t)ptTexture->tDesc.tDimensions.x * (uint32_t)ptTexture->tDesc.tDimensions.y * pl__format_stride(ptTexture->tDesc.eFormat));
}

void
pl_graphics_end_render_pass(plCommandBuffer* ptEncoder)
{
}

void
pl_graphics_bind_vertex_buffers(plCommandBuffer* ptCmdBuffer, uint32_t uFirst, uint32_t uCount, const plBufferHandle* ptHandles, const size_t* pszOffsets)
{
    PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_bind_vertex_buffer(plCommandBuffer* ptCommandBuffer, plBufferHandle tHandle)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    ptCommandBuffer->tCurrentVertexBuffer = tHandle;
}

void
pl_graphics_draw(plCommandBuffer* ptEncoder, uint32_t uCount, const plDraw *atDraws)
{
    PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_draw_indexed(plCommandBuffer* ptCommandBuffer, uint32_t uCount, const plDrawIndex *atDraws)
{

    for(uint32_t uDrawIndex = 0; uDrawIndex < uCount; uDrawIndex++)
    {
        ptCommandBuffer->uCurrentStreamItem++;
        pl_sb_add(ptCommandBuffer->sbtStream);
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].eType  = PL_CPU_COMMAND_BUFFER_ITEM_TYPE_DRAW_INDEXED;
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tDraw  = atDraws[uDrawIndex];
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tShader = ptCommandBuffer->tCurrentShader;
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tVertexBuffer = ptCommandBuffer->tCurrentVertexBuffer;
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tRenderInfo = ptCommandBuffer->tCurrentRenderInfo;
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].atCurrentDescriptorSets[0] = ptCommandBuffer->atCurrentDescriptorSets[0];
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].atCurrentDescriptorSets[1] = ptCommandBuffer->atCurrentDescriptorSets[1];
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].atCurrentDescriptorSets[2] = ptCommandBuffer->atCurrentDescriptorSets[2];
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].atCurrentDescriptorSets[3] = ptCommandBuffer->atCurrentDescriptorSets[3];
    }
}

void
pl_graphics_bind_shader(plCommandBuffer* ptCommandBuffer, plShaderHandle tHandle)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    ptCommandBuffer->tCurrentShader = tHandle;
}

void
pl_graphics_bind_compute_shader(plCommandBuffer* ptEncoder, plComputeShaderHandle tHandle)
{
    PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_draw_stream(plCommandBuffer* ptEncoder, uint32_t uAreaCount, plDrawArea *atAreas)
{
    PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_set_depth_bias(plCommandBuffer* ptEncoder, float fDepthBiasConstantFactor, float fDepthBiasClamp, float fDepthBiasSlopeFactor)
{
}

void
pl_graphics_set_viewport(plCommandBuffer* ptCommandBuffer, const plRenderViewport* ptViewport)
{
    ptCommandBuffer->uCurrentStreamItem++;
    pl_sb_add(ptCommandBuffer->sbtStream);
    ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].eType  = PL_CPU_COMMAND_BUFFER_ITEM_TYPE_SET_VIEWPORT;
    ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tViewport = *ptViewport;
}

void
pl_graphics_set_scissor_region(plCommandBuffer* ptCommandBuffer, const plScissor* ptScissor)
{
    ptCommandBuffer->uCurrentStreamItem++;
    pl_sb_add(ptCommandBuffer->sbtStream);
    ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].eType  = PL_CPU_COMMAND_BUFFER_ITEM_TYPE_SET_SCISSOR;
    ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tScissor = *ptScissor;
}

plDeviceMemoryAllocation
pl_graphics_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryFlags tMemoryFlags, uint32_t uTypeFilter, const char* pcName)
{
    plDeviceMemoryAllocation tAllocation = {0};
    tAllocation.ulSize = szSize;
    tAllocation.pHostMapped = PL_ALLOC(szSize);
    tAllocation.uHandle = (uint64_t)tAllocation.pHostMapped;
    memset(tAllocation.pHostMapped, 0, szSize);
    return tAllocation;
}

void
pl_graphics_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    free(ptBlock->pHostMapped);
    ptBlock->pHostMapped = NULL;
}

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

plGraphicsBackend
pl_graphics_get_backend(void)
{
    return PL_GRAPHICS_BACKEND_CPU;
}

const char*
pl_graphics_get_backend_string(void)
{
    return "Archery Rasterizer (CPU)";
}

bool
pl_graphics_initialize(const plGraphicsInit* ptDesc)
{
    static plGraphics gtGraphics = {0};
    gptGraphics = &gtGraphics;

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

    // save context for hot-reloads
    gptDataRegistry->set_data("plGraphics", gptGraphics);

    gptGraphics->bValidationActive = ptDesc->eFlags & PL_GRAPHICS_INIT_FLAGS_VALIDATION_ENABLED;

    gptGraphics->bDebugMessengerActive = gptGraphics->bValidationActive;

    // set frames in flight (if zero, use a default of 2)
    gptGraphics->uFramesInFlight = pl_min(pl_max(ptDesc->uFramesInFlight, 2), PL_MAX_FRAMES_IN_FLIGHT);

    return true;
}

void
pl_graphics_enumerate_devices(plDeviceInfo *atDeviceInfo, uint32_t* puDeviceCount)
{
    *puDeviceCount = 1;

    if(atDeviceInfo == NULL)
        return;

    plIO* ptIOCtx = gptIOI->get_io();

    strncpy(atDeviceInfo[0].acName, "PL CPU Rasterizer", 256);
    atDeviceInfo[0].eVendorId = PL_VENDOR_ID_NONE;
    atDeviceInfo[0].eType = PL_DEVICE_TYPE_CPU;
    atDeviceInfo[0].eCapabilities = PL_DEVICE_CAPABILITY_BIND_GROUP_INDEXING | PL_DEVICE_CAPABILITY_SAMPLER_ANISOTROPY | PL_DEVICE_CAPABILITY_SWAPCHAIN | PL_DEVICE_CAPABILITY_MULTIPLE_VIEWPORTS;
}

plDevice*
pl_graphics_create_device(const plDeviceInit* ptInit)
{

    plDevice* ptDevice = PL_ALLOC(sizeof(plDevice));
    memset(ptDevice, 0, sizeof(plDevice));
    ptDevice->tInit = *ptInit;

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

    pl_sb_resize(ptDevice->sbtGarbage, gptGraphics->uFramesInFlight);
    for(uint32_t i = 0; i < gptGraphics->uFramesInFlight; i++)
    {
        plFrameContext tFrame = {
            .uNextValue = 0
        };
        pl_sb_push(ptDevice->sbtFrames, tFrame);
    }

    if(ptDevice->tInit.szDynamicBufferBlockSize == 0) ptDevice->tInit.szDynamicBufferBlockSize = 134217728;
    if(ptDevice->tInit.szDynamicDataMaxSize == 0)     ptDevice->tInit.szDynamicDataMaxSize = 256;


    // const size_t szMaxDynamicBufferDescriptors = ptDevice->tInit.szDynamicBufferBlockSize / ptDevice->tInit.szDynamicDataMaxSize;

    // ptDevice->szDynamicArgumentBufferHeapSize = sizeof(uint64_t) * szMaxDynamicBufferDescriptors;
    // ptDevice->szDynamicArgumentBufferSize = sizeof(uint64_t) * 256;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~device memory allocators~~~~~~~~~~~~~~~~~~~~~~~~~

    static plInternalDeviceAllocatorData tAllocatorData = {0};
    static plDeviceMemoryAllocatorI tAllocator = {0};
    tAllocatorData.ptAllocator = &tAllocator;
    tAllocatorData.ptDevice = ptDevice;
    tAllocator.allocate = pl_allocate_staging_dynamic;
    tAllocator.free = pl_free_staging_dynamic;
    tAllocator.ptInst = (struct plDeviceMemoryAllocatorO*)&tAllocatorData;
    ptDevice->ptDynamicAllocator = &tAllocator;
    
    return ptDevice;
}

plSurface*
pl_graphics_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));

    plWindowSurfaceDesc tSurfaceDesc = {
        .tFormat     = PL_WINDOW_SURFACE_FORMAT_B8G8R8A8_UNORM,
        .uImageCount = 2
    };
    gptWindows->create_surface(ptWindow, &tSurfaceDesc, &ptSurface->ptWindowSurface);
    return ptSurface;
}

plSwapchain*
pl_graphics_create_swapchain(plDevice* ptDevice, plSurface* ptSurface, const plSwapchainInit* ptInit)
{
    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));
    ptSwap->ptSurface = ptSurface;
    ptSwap->uImageCount = gptGraphics->uFramesInFlight;
    ptSwap->tInfo.eFormat = PL_FORMAT_B8G8R8A8_UNORM;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->ptDevice = ptDevice;
    ptSwap->tInfo.eSampleCount = pl_min(ptInit->eSampleCount, ptSwap->ptDevice->tInfo.eMaxSampleCount);
    if(ptSwap->tInfo.eSampleCount == 0)
        ptSwap->tInfo.eSampleCount = 1;
    ptSwap->tInfo.uWidth = (uint32_t)gptIO->tMainViewportSize.x;
    ptSwap->tInfo.uHeight = (uint32_t)gptIO->tMainViewportSize.y;

    for(uint32_t i = 0; i < ptSwap->uImageCount; i++)
    {
        plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
        plTexture* ptTexture = &ptDevice->sbtTexturesCold[tHandle.uIndex];
        ptTexture->tDesc.eFormat = PL_FORMAT_B8G8R8A8_UNORM;
        ptTexture->tDesc.tDimensions.x = (float)ptInit->uWidth;
        ptTexture->tDesc.tDimensions.y = (float)ptInit->uHeight;
        pl_sb_push(ptSwap->sbtSwapchainTextureViews, tHandle);
    }
    return ptSwap;
}

void
pl_graphics_begin_frame(plDevice* ptDevice)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    pl__garbage_collect(ptDevice);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

bool
pl_graphics_acquire_swapchain_image(plSwapchain* ptSwap)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plDevice* ptDevice = ptSwap->ptDevice;

    if(gptWindows->acquire_surface_image(ptSwap->ptSurface->ptWindowSurface, &ptSwap->tCurrentSurfaceImage))
    {
        plCpuTexture* ptCpuTexture = &ptDevice->sbtTexturesHot[ptSwap->sbtSwapchainTextureViews[ptSwap->tCurrentSurfaceImage.uImageIndex].uIndex];
        plTexture* ptTexture = &ptDevice->sbtTexturesCold[ptSwap->sbtSwapchainTextureViews[ptSwap->tCurrentSurfaceImage.uImageIndex].uIndex];
        ptTexture->tDesc.tDimensions.x = (float)ptSwap->tCurrentSurfaceImage.uWidth;
        ptTexture->tDesc.tDimensions.y = (float)ptSwap->tCurrentSurfaceImage.uHeight;
        ptCpuTexture->pData = ptSwap->tCurrentSurfaceImage.pPixels;
        ptSwap->uCurrentImageIndex = ptSwap->tCurrentSurfaceImage.uImageIndex;
    }

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

void
pl_graphics_end_command_recording(plCommandBuffer* ptCommandBuffer)
{
}

bool
pl_graphics_present(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo, plSwapchain **ptSwaps, uint32_t uSwapchainCount)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    pl_graphics_submit_command_buffer(ptCmdBuffer, ptSubmitInfo);

    gptWindows->present_surface_image(ptSwaps[0]->ptSurface->ptWindowSurface, ptSwaps[0]->tCurrentSurfaceImage.uImageIndex);
    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    ptCmdBuffer->ptDevice->sbtFrames[gptGraphics->uCurrentFrameIndex].uCurrentBufferIndex = 0;
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

void
pl_graphics_recreate_swapchain(plSwapchain* ptSwap, const plSwapchainInit* ptInit)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    bool bMSAAChange = ptSwap->tInfo.eSampleCount != ptInit->eSampleCount;

    // gptGraphics->uCurrentFrameIndex = 0;
    ptSwap->tInfo.bVSync = ptInit->bVSync;
    ptSwap->tInfo.uWidth = ptInit->uWidth;
    ptSwap->tInfo.uHeight = ptInit->uHeight;
    ptSwap->tInfo.eSampleCount = pl_min(ptInit->eSampleCount, ptSwap->ptDevice->tInfo.eMaxSampleCount);
    if(ptSwap->tInfo.eSampleCount == 0)
        ptSwap->tInfo.eSampleCount = 1;

    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

void
pl_graphics_flush_device(plDevice* ptDevice)
{
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
    gptWindows->destroy_surface(&ptSwap->ptSurface->ptWindowSurface);
    pl__cleanup_common_swapchain(ptSwap);
}

void
pl_graphics_cleanup_device(plDevice* ptDevice)
{
    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBindGroupsHot); i++)
    {
        if(ptDevice->sbtBindGroupsHot[i].atDescriptors)
        {
            PL_FREE(ptDevice->sbtBindGroupsHot[i].atDescriptors);
            ptDevice->sbtBindGroupsHot[i].atDescriptors = NULL;
        }
    }
    pl_sb_free(ptDevice->sbtShadersHot);
    pl_sb_free(ptDevice->sbtComputeShadersHot);
    pl_sb_free(ptDevice->sbtBuffersHot);
    pl_sb_free(ptDevice->sbtTexturesHot);
    pl_sb_free(ptDevice->sbtSamplersHot);
    pl_sb_free(ptDevice->sbtBindGroupsHot);
    pl_sb_free(ptDevice->sbtBindGroupLayoutsHot);
    pl__cleanup_common_device(ptDevice);
}

void
pl_graphics_begin_compute_pass(plCommandBuffer* ptCmdBuffer, const plPassResources* ptResources)
{
    // PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_end_compute_pass(plCommandBuffer* ptEncoder)
{
    // PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_dispatch(plCommandBuffer* ptEncoder, uint32_t uDispatchCount, const plDispatch *atDispatches)
{
    PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_bind_compute_bind_groups(plCommandBuffer* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst,
    uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
    PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_bind_graphics_bind_groups(plCommandBuffer* ptCommandBuffer, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    for(uint32_t i = 0; i < uCount; i++)
    {
        plCpuBindGroup* ptCpuBindGroup = &ptDevice->sbtBindGroupsHot[atBindGroups[i].uIndex];

        ptCommandBuffer->atCurrentDescriptorSets[uFirst + i].atDescriptors = ptCpuBindGroup->atDescriptors;
    }

    if(uDynamicBindingCount > 0)
    {
        plFrameContext* ptFrame = pl__get_frame_resources(ptDevice);
        ptCommandBuffer->atCurrentDescriptorSets[3].atDescriptors[0].eType = PL_DESCRIPTOR_TYPE_BUFFER;
        ptCommandBuffer->atCurrentDescriptorSets[3].atDescriptors[0].puData = (uint8_t*)ptDynamicBinding[0].pcData;
    }
}

#define SUBPIXEL_BITS  8
#define SUBPIXEL_SCALE (1 << SUBPIXEL_BITS)
#define SUBPIXEL_HALF  (SUBPIXEL_SCALE / 2)

void
pl_graphics_submit_command_buffer(plCommandBuffer* ptCommandBuffer, const plSubmitInfo* ptSubmitInfo)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    uint32_t uDrawCount = pl_sb_size(ptCommandBuffer->sbtStream);
    plRenderViewport tCurrentViewport = {0};
    plScissor tCurrentScissor = {0};
    for(uint32_t uDrawIndex = 0; uDrawIndex < uDrawCount; uDrawIndex++)
    {
        plCommandBufferItem* ptCmdBufferItem = &ptCommandBuffer->sbtStream[uDrawIndex];

        if(ptCmdBufferItem->eType == PL_CPU_COMMAND_BUFFER_ITEM_TYPE_SET_VIEWPORT)
        {
            tCurrentViewport = ptCmdBufferItem->tViewport;
        }
        else if(ptCmdBufferItem->eType == PL_CPU_COMMAND_BUFFER_ITEM_TYPE_SET_SCISSOR)
        {
            tCurrentScissor = ptCmdBufferItem->tScissor;
        }
        else if(ptCmdBufferItem->eType == PL_CPU_COMMAND_BUFFER_ITEM_TYPE_COPY_BUFFER_TO_TEXTURE)
        {
            plCpuBuffer* ptBuffer = &ptDevice->sbtBuffersHot[ptCmdBufferItem->tBufferHandle.uIndex];
            plTexture* ptTexture = &ptDevice->sbtTexturesCold[ptCmdBufferItem->tTextureHandle.uIndex];
            plCpuTexture* ptCpuTexture = &ptDevice->sbtTexturesHot[ptCmdBufferItem->tTextureHandle.uIndex];
            plBufferImageCopy* ptCopy = &ptCmdBufferItem->tBufferImageCopy;
            uint8_t* puBufferSrc = &((uint8_t*)ptBuffer->pData)[ptCopy->szBufferOffset];
            uint8_t* puBufferDst = (uint8_t*)ptCpuTexture->pData;

            const size_t szStride = pl__format_stride(ptTexture->tDesc.eFormat);
            const int iDstWidth = (int)ptTexture->tDesc.tDimensions.x;
            const int iSrcRowLength = ptCopy->uBufferRowLength ? (int)ptCopy->uBufferRowLength : (int)ptCopy->uImageWidth;
            for(int y = 0; y < (int)ptCopy->uImageHeight; y++)
            {
                const int dstY = ptCopy->iImageOffsetY + y;
                for(int x = 0; x < (int)ptCopy->uImageWidth; x++)
                {
                    const int dstX = ptCopy->iImageOffsetX + x;
                    memcpy(&puBufferDst[(dstY * iDstWidth + dstX) * szStride], &puBufferSrc[(y * iSrcRowLength + x) * szStride], szStride);
                }
            }
            
        }
        else if(ptCmdBufferItem->eType == PL_CPU_COMMAND_BUFFER_ITEM_TYPE_DRAW_INDEXED)
        {
            const plShader* ptShader = &ptDevice->sbtShadersCold[ptCmdBufferItem->tShader.uIndex];
            const plShader* ptPipeline = &ptDevice->sbtShadersCold[ptCmdBufferItem->tShader.uIndex];
            const plCpuShader* ptCpuPipeline = &ptDevice->sbtShadersHot[ptCmdBufferItem->tShader.uIndex];
            plTexture* ptTexture = &ptDevice->sbtTexturesCold[ptCmdBufferItem->tRenderInfo.atColorAttachments[0].tTexture.uIndex];
            const int fbWidth  = (int)ptTexture->tDesc.tDimensions.x;
            const int fbHeight = (int)ptTexture->tDesc.tDimensions.y;
        
            plVec2 vertexP = {0};  

            const void* pVerticies = ptDevice->sbtBuffersHot[ptCmdBufferItem->tVertexBuffer.uIndex].pData;
            const uint32_t* puIndexBufferData = ptDevice->sbtBuffersHot[ptCmdBufferItem->tDraw.tIndexBuffer.uIndex].pData;
            const uint32_t stride = ptShader->tDesc.atVertexBufferLayouts[0].uByteStride;
            plCpuTexture* ptCpuTexture = &ptDevice->sbtTexturesHot[ptCmdBufferItem->tRenderInfo.atColorAttachments[0].tTexture.uIndex];
            const uint32_t uVertexCount = (uint32_t)ptDevice->sbtBuffersCold[ptCmdBufferItem->tVertexBuffer.uIndex].tDesc.szByteSize / stride;
            pl_sb_reset(ptCommandBuffer->sbtVertexCache);
            pl_sb_resize(ptCommandBuffer->sbtVertexCache, uVertexCount);
            memset(ptCommandBuffer->sbtVertexCache, 0, uVertexCount * sizeof(plVertexCache));

            for(uint32_t i = 0; i < ptCmdBufferItem->tDraw.uIndexCount; i += 3)
            {

                const uint32_t uIndex0 = ptCmdBufferItem->tDraw.uVertexStart + puIndexBufferData[ptCmdBufferItem->tDraw.uIndexStart + i];
                const uint32_t uIndex1 = ptCmdBufferItem->tDraw.uVertexStart + puIndexBufferData[ptCmdBufferItem->tDraw.uIndexStart + i + 1];
                const uint32_t uIndex2 = ptCmdBufferItem->tDraw.uVertexStart + puIndexBufferData[ptCmdBufferItem->tDraw.uIndexStart + i + 2];

                // type casting void buffer
                const char* pcVtxBuffer = (char*)pVerticies;

                // vertex shader stage
                // setting vertex ID
                plVertexShaderBuiltIns tVSBuiltIns0 = {
                    .uVertexID = uIndex0,
                    .atLayouts = ptShader->tDesc.atVertexBufferLayouts
                };
                plVertexShaderBuiltIns tVSBuiltIns1 = {
                    .uVertexID = uIndex1,
                    .atLayouts = ptShader->tDesc.atVertexBufferLayouts
                };
                plVertexShaderBuiltIns tVSBuiltIns2 = {
                    .uVertexID = uIndex2,
                    .atLayouts = ptShader->tDesc.atVertexBufferLayouts
                };

                if(!ptCommandBuffer->sbtVertexCache[uIndex0].bValid)
                {
                    ptCommandBuffer->sbtVertexCache[uIndex0].tPosition = ptCpuPipeline->tVertexShader(tVSBuiltIns0, ptCmdBufferItem->atCurrentDescriptorSets, &pcVtxBuffer[uIndex0 * stride], &ptCommandBuffer->sbtVertexCache[uIndex0].tVaryings);
                    ptCommandBuffer->sbtVertexCache[uIndex0].bValid = true;
                }

                if(!ptCommandBuffer->sbtVertexCache[uIndex1].bValid)
                {
                    ptCommandBuffer->sbtVertexCache[uIndex1].tPosition = ptCpuPipeline->tVertexShader(tVSBuiltIns1, ptCmdBufferItem->atCurrentDescriptorSets, &pcVtxBuffer[uIndex1 * stride], &ptCommandBuffer->sbtVertexCache[uIndex1].tVaryings);
                    ptCommandBuffer->sbtVertexCache[uIndex1].bValid = true;
                }

                if(!ptCommandBuffer->sbtVertexCache[uIndex2].bValid)
                {
                    ptCommandBuffer->sbtVertexCache[uIndex2].tPosition = ptCpuPipeline->tVertexShader(tVSBuiltIns2, ptCmdBufferItem->atCurrentDescriptorSets, &pcVtxBuffer[uIndex2 * stride], &ptCommandBuffer->sbtVertexCache[uIndex2].tVaryings);
                    ptCommandBuffer->sbtVertexCache[uIndex2].bValid = true;
                }

                // defining varying data
                plVaryingData tVaryingData0 = ptCommandBuffer->sbtVertexCache[uIndex0].tVaryings;
                plVaryingData tVaryingData1 = ptCommandBuffer->sbtVertexCache[uIndex1].tVaryings;
                plVaryingData tVaryingData2 = ptCommandBuffer->sbtVertexCache[uIndex2].tVaryings;

                plVaryingData tVaryingTemplate = {0};

                for(uint32_t j = 0; j < 16; j++)
                    tVaryingTemplate._auOffset[j] = tVaryingData0._auOffset[j];

                int iVaryingCount = 0;
                for(int varyIndex = 0; varyIndex < 16; varyIndex++)
                {
                    if(tVaryingData0.atTypes[varyIndex] == PL_VARYING_TYPE_NONE)
                        break;
                    iVaryingCount++;
                }

                // frame buffer space
                plVec2 tVertex0 = ptCommandBuffer->sbtVertexCache[uIndex0].tPosition;
                plVec2 tVertex1 = ptCommandBuffer->sbtVertexCache[uIndex1].tPosition;
                plVec2 tVertex2 = ptCommandBuffer->sbtVertexCache[uIndex2].tPosition;

                const float viewportX = tCurrentViewport.fX;
                const float viewportY = tCurrentViewport.fY;
                const float viewportWidth  = tCurrentViewport.fWidth;
                const float viewportHeight = tCurrentViewport.fHeight;

                tVertex0.x = viewportX + viewportWidth * (0.5f + 0.5f * tVertex0.x);
                tVertex1.x = viewportX + viewportWidth * (0.5f + 0.5f * tVertex1.x);
                tVertex2.x = viewportX + viewportWidth * (0.5f + 0.5f * tVertex2.x);

                tVertex0.y = viewportY + viewportHeight * (0.5f + 0.5f * tVertex0.y);
                tVertex1.y = viewportY + viewportHeight * (0.5f + 0.5f * tVertex1.y);
                tVertex2.y = viewportY + viewportHeight * (0.5f + 0.5f * tVertex2.y);

                const int64_t x0 = llroundf(tVertex0.x * SUBPIXEL_SCALE);
                const int64_t y0 = llroundf(tVertex0.y * SUBPIXEL_SCALE);

                const int64_t x1 = llroundf(tVertex1.x * SUBPIXEL_SCALE);
                const int64_t y1 = llroundf(tVertex1.y * SUBPIXEL_SCALE);

                const int64_t x2 = llroundf(tVertex2.x * SUBPIXEL_SCALE);
                const int64_t y2 = llroundf(tVertex2.y * SUBPIXEL_SCALE);

                const int64_t ABC = pl__edge_function_fixed(x0, y0, x1, y1, x2, y2);

                // PL_ASSERT(ABC != 0);

                if(ABC == 0)
                    continue;

                const int64_t orientation = ABC > 0 ? 1 : -1;
                const float invABC = 1.0f / (float)ABC;

                // edge function for entire triangle 
                // float ABC = (float)pl__edge_function(tVertex0, tVertex1, tVertex2);

                const int viewportMinX = (int)floorf(viewportX);
                const int viewportMinY = (int)floorf(viewportY);

                const int viewportMaxX = (int)ceilf(viewportX + viewportWidth) - 1;
                const int viewportMaxY = (int)ceilf(viewportY + viewportHeight) - 1;

                // Bounding box with clamping
                int minX = pl_max(viewportMinX, pl_min3((int)tVertex0.x, (int)tVertex1.x, (int)tVertex2.x) - 1);
                int minY = pl_max(viewportMinY, pl_min3((int)tVertex0.y, (int)tVertex1.y, (int)tVertex2.y) - 1);
                int maxX = pl_min(viewportMaxX, pl_max3((int)tVertex0.x, (int)tVertex1.x, (int)tVertex2.x) + 1);
                int maxY = pl_min(viewportMaxY, pl_max3((int)tVertex0.y, (int)tVertex1.y, (int)tVertex2.y) + 1);

                minX = pl_max(minX, tCurrentScissor.iOffsetX);
                minY = pl_max(minY, tCurrentScissor.iOffsetY);
                maxX = pl_min(maxX, tCurrentScissor.iOffsetX + (int)tCurrentScissor.uWidth - 1);
                maxY = pl_min(maxY, tCurrentScissor.iOffsetY + (int)tCurrentScissor.uHeight - 1);

                // Precompute edge function deltas
                const int64_t ABa = y0 - y1;
                const int64_t ABb = x1 - x0;
                const int64_t ABc = x0 * y1 - x1 * y0;

                const int64_t BCa = y1 - y2;
                const int64_t BCb = x2 - x1;
                const int64_t BCc = x1 * y2 - x2 * y1;

                const int64_t CAa = y2 - y0;
                const int64_t CAb = x0 - x2;
                const int64_t CAc = x2 * y0 - x0 * y2;

                const int64_t sampleX = (int64_t)minX * SUBPIXEL_SCALE + SUBPIXEL_HALF;
                const int64_t sampleY = (int64_t)minY * SUBPIXEL_SCALE + SUBPIXEL_HALF;

                int64_t ABP = ABa * sampleX + ABb * sampleY + ABc;
                int64_t BCP = BCa * sampleX + BCb * sampleY + BCc;
                int64_t CAP = CAa * sampleX + CAb * sampleY + CAc;

                // if(ABC == 0.0f)
                //     continue;

                // const float invABC = 1.0f / ABC;

                // With your Y-down framebuffer mapping, verify this sign once.
                const bool bFrontFacing = ABC < 0.0f;

                if(ptPipeline->tDesc.tGraphicsState.eCullMode == PL_CULL_MODE_CULL_BACK && !bFrontFacing)
                    continue;

                if(ptPipeline->tDesc.tGraphicsState.eCullMode == PL_CULL_MODE_CULL_FRONT && bFrontFacing)
                    continue;

                // const float orientation = ABC > 0.0f ? 1.0f : -1.0f;

                // Reverse edge direction when triangle winding is reversed,
                // so the top-left test is always evaluated in the same orientation.
                const bool ABTopLeft = orientation > 0.0f ? pl__is_top_left_edge_fixed(x0, y0, x1, y1) : pl__is_top_left_edge_fixed(x1, y1, x0, y0);
                const bool BCTopLeft = orientation > 0.0f ? pl__is_top_left_edge_fixed(x1, y1, x2, y2) : pl__is_top_left_edge_fixed(x2, y2, x1, y1);
                const bool CATopLeft = orientation > 0.0f ? pl__is_top_left_edge_fixed(x2, y2, x0, y0) : pl__is_top_left_edge_fixed(x0, y0, x2, y2);


                for(vertexP.y = (float)minY; vertexP.y <= (float)maxY; vertexP.y++)
                {
                    int64_t rowABP = ABP;
                    int64_t rowBCP = BCP;
                    int64_t rowCAP = CAP;
                
                    for(vertexP.x = (float)minX; vertexP.x <= (float)maxX; vertexP.x++)
                    {

                        // if(!(vertexP.x >= tCurrentScissor.iOffsetX && vertexP.x < tCurrentScissor.iOffsetX + tCurrentScissor.uWidth && vertexP.y >= tCurrentScissor.iOffsetY && vertexP.y < tCurrentScissor.iOffsetY + tCurrentScissor.uHeight))
                        //     continue;
                        const int64_t eAB = rowABP * orientation;
                        const int64_t eBC = rowBCP * orientation;
                        const int64_t eCA = rowCAP * orientation;

                        const bool inside = (eAB > 0.0f || (eAB == 0.0f && ABTopLeft)) && (eBC > 0.0f || (eBC == 0.0f && BCTopLeft)) && (eCA > 0.0f || (eCA == 0.0f && CATopLeft));

                        // if(rowABP <= 0 && rowBCP <= 0 && rowCAP <= 0) // back culling
                        // if((rowABP >= 0 && rowBCP >= 0 && rowCAP >= 0) || (rowABP <= 0 && rowBCP <= 0 && rowCAP <= 0)) // no culling
                        if(inside) // no culling
                        {
                            const float weightA = (float)((double)rowBCP * invABC);
                            const float weightB = (float)((double)rowCAP * invABC);
                            const float weightC = (float)((double)rowABP * invABC);
                            
                            plPixelShaderBuiltIns tBuiltIns = {
                                .gl_FragCoord.xy = {
                                    vertexP.x + 0.5f,
                                    vertexP.y + 0.5f
                                }
                            };

                            // Varying system
                            int varyDataOffset = 0;
                            plVaryingData blendedVaryingData = tVaryingTemplate;

                            for(int varyIndex = 0; varyIndex < iVaryingCount; varyIndex++)
                            {
                                if(tVaryingData0.atTypes[varyIndex] == PL_VARYING_TYPE_VEC2)
                                {
                                    // 1st input 
                                    // Vec2 blending
                                    const plVec2 twoFloats0 = *(plVec2*)&tVaryingData0.acVaryingData[varyDataOffset];
                                    const plVec2 twoFloats1 = *(plVec2*)&tVaryingData1.acVaryingData[varyDataOffset];
                                    const plVec2 twoFloats2 = *(plVec2*)&tVaryingData2.acVaryingData[varyDataOffset];

                                    plVec2 blendedVec2 = {
                                        .x = ((float)(twoFloats0.x * weightA) + (float)(twoFloats1.x * weightB) + (float)(twoFloats2.x * weightC)),
                                        .y = ((float)(twoFloats0.y * weightA) + (float)(twoFloats1.y * weightB) + (float)(twoFloats2.y * weightC)),
                                    };
                                    memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &blendedVec2, sizeof(plVec2));
                                    varyDataOffset += sizeof(plVec2);
                                }
                                if(tVaryingData0.atTypes[varyIndex] == PL_VARYING_TYPE_VEC3)
                                {
                                    // 2nd input 
                                    // Vec3 blending 
                                    const plVec3* ptVecThree0 = (plVec3*)&tVaryingData0.acVaryingData[varyDataOffset];
                                    const plVec3* ptVecThree1 = (plVec3*)&tVaryingData1.acVaryingData[varyDataOffset];
                                    const plVec3* ptVecThree2 = (plVec3*)&tVaryingData2.acVaryingData[varyDataOffset];

                                    plVec4 tBlendedVecThree = {
                                        .x = ((float)(ptVecThree0->x * weightA) + (float)(ptVecThree1->x * weightB) + (float)(ptVecThree2->x * weightC)),
                                        .y = ((float)(ptVecThree0->y * weightA) + (float)(ptVecThree1->y * weightB) + (float)(ptVecThree2->y * weightC)),
                                        .z = ((float)(ptVecThree0->z * weightA) + (float)(ptVecThree1->z * weightB) + (float)(ptVecThree2->z * weightC))
                                    };
                                    memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &tBlendedVecThree, sizeof(plVec3));
                                    varyDataOffset += sizeof(plVec3);
                                }
                                if(tVaryingData0.atTypes[varyIndex] == PL_VARYING_TYPE_VEC4)
                                {
                                    // 3rd input
                                    // Vec4 blending 
                                    const plVec4* ptColor0 = (plVec4*)&tVaryingData0.acVaryingData[varyDataOffset];
                                    const plVec4* ptColor1 = (plVec4*)&tVaryingData1.acVaryingData[varyDataOffset];
                                    const plVec4* ptColor2 = (plVec4*)&tVaryingData2.acVaryingData[varyDataOffset];

                                    plVec4 tBlendedColor = {
                                        .x = ((float)(ptColor0->x * weightA) + (float)(ptColor1->x * weightB) + (float)(ptColor2->x * weightC)),
                                        .y = ((float)(ptColor0->y * weightA) + (float)(ptColor1->y * weightB) + (float)(ptColor2->y * weightC)),
                                        .z = ((float)(ptColor0->z * weightA) + (float)(ptColor1->z * weightB) + (float)(ptColor2->z * weightC)),
                                        .w = ((float)(ptColor0->w * weightA) + (float)(ptColor1->w * weightB) + (float)(ptColor2->w * weightC))
                                    };
                                    memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &tBlendedColor, sizeof(plVec4));
                                    varyDataOffset += sizeof(plVec4);
                                }
                                if(tVaryingData0.atTypes[varyIndex] == PL_VARYING_TYPE_FLOAT)
                                {
                                    // 4th input 
                                    // float blending 
                                    const float tData0 = *(float*)&tVaryingData0.acVaryingData[varyDataOffset];
                                    const float tData1 = *(float*)&tVaryingData1.acVaryingData[varyDataOffset];
                                    const float tData2 = *(float*)&tVaryingData2.acVaryingData[varyDataOffset];

                                    float fBlendedData2 = ((float)(tData0 * weightA) + (float)(tData1 * weightB) + (float)(tData2 * weightC));

                                    memcpy(&blendedVaryingData.acVaryingData[varyDataOffset], &fBlendedData2, sizeof(float));
                                    varyDataOffset += sizeof(float);
                                }
                            }

                            // scissor check
                            // if(vertexP.x >= tCurrentScissor.iOffsetX && vertexP.x < tCurrentScissor.iOffsetX + tCurrentScissor.uWidth && vertexP.y >= tCurrentScissor.iOffsetY && vertexP.y < tCurrentScissor.iOffsetY + tCurrentScissor.uHeight)
                            {
                                // run pixel shader
                                plVec4 tFinalColor = ptCpuPipeline->tPixelShader(tBuiltIns, ptCmdBufferItem->atCurrentDescriptorSets, &blendedVaryingData);
                                

                                pl_set_pixel(ptCpuTexture, vertexP, tFinalColor, (uint32_t)fbWidth, (uint32_t)fbHeight);
                            }
                        }
                        // Incrementally update edge functions for next pixel in row
                        rowABP += ABa * SUBPIXEL_SCALE;
                        rowBCP += BCa * SUBPIXEL_SCALE;
                        rowCAP += CAa * SUBPIXEL_SCALE;
                    }
                    // Incrementally update edge functions for next row
                    ABP += ABb * SUBPIXEL_SCALE;
                    BCP += BCb * SUBPIXEL_SCALE;
                    CAP += CAb * SUBPIXEL_SCALE;
                }
            }
        }
    }
}

void
pl_graphics_wait_on_command_buffer(plCommandBuffer* ptCmdBuffer)
{
}

void
pl_graphics_return_command_buffer(plCommandBuffer* ptCmdBuffer)
{
    pl_sb_reset(ptCmdBuffer->sbtStream);
    ptCmdBuffer->uCurrentStreamItem = UINT32_MAX;
    ptCmdBuffer->ptNext = ptCmdBuffer->ptPool->ptCommandBufferFreeList;
    ptCmdBuffer->ptPool->ptCommandBufferFreeList = ptCmdBuffer;
}

plBindGroupPool*
pl_graphics_create_bind_group_pool(plDevice* ptDevice, const plBindGroupPoolDesc* ptDesc)
{
    plBindGroupPool* ptPool = PL_ALLOC(sizeof(plBindGroupPool));
    memset(ptPool, 0, sizeof(plBindGroupPool));
    return ptPool;
}

void
pl_graphics_reset_bind_group_pool(plBindGroupPool* ptPool)
{
}

void
pl_graphics_cleanup_bind_group_pool(plBindGroupPool* ptPool)
{
    PL_FREE(ptPool);
}

plCommandPool *
pl_graphics_create_command_pool(plDevice* ptDevice, const plCommandPoolDesc* ptDesc)
{
    plCommandPool* ptPool = PL_ALLOC(sizeof(plCommandPool));
    memset(ptPool, 0, sizeof(plCommandPool));
    ptPool->ptDevice = ptDevice;
    return ptPool;
}

void
pl_graphics_cleanup_command_pool(plCommandPool* ptPool)
{
    plCommandBuffer* ptCmdBuffer = ptPool->ptCommandBufferFreeList;
    while(ptCmdBuffer)
    {
        plCommandBuffer* ptNextCmdBuffer = ptCmdBuffer->ptNext;
        pl_sb_free(ptCmdBuffer->sbtStream);
        PL_FREE(ptCmdBuffer);
        ptCmdBuffer = ptNextCmdBuffer;
        
    }
    PL_FREE(ptPool);
}

void
pl_graphics_reset_command_pool(plCommandPool* ptPool, plCommandPoolResetFlags tFlags)
{
}

void
pl_graphics_reset_command_buffer(plCommandBuffer* ptCommandBuffer)
{
}

plCommandBuffer*
pl_graphics_request_command_buffer(plCommandPool* ptPool, const char* pcDebugName)
{
    plCommandBuffer* ptCommandBuffer = ptPool->ptCommandBufferFreeList;
    if (ptCommandBuffer)
    {
        ptPool->ptCommandBufferFreeList = ptCommandBuffer->ptNext;
    }
    else
    {
        ptCommandBuffer = PL_ALLOC(sizeof(plCommandBuffer));
        memset(ptCommandBuffer, 0, sizeof(plCommandBuffer));
        ptCommandBuffer->atCurrentDescriptorSets[3].atDescriptors = PL_ALLOC(sizeof(plDescriptor) * 1);
    }
    ptCommandBuffer->ptPool = ptPool;
    ptCommandBuffer->ptDevice = ptPool->ptDevice;
    return ptCommandBuffer;
}

void
pl_graphics_copy_buffer(plCommandBuffer* ptCommandBuffer, plBufferHandle tSource, plBufferHandle tDestination, uint64_t uSourceOffset, uint64_t uDestinationOffset, size_t szSize)
{
    PL_ASSERT(false && "NOT IMPLEMENTED");
}

void
pl_graphics_copy_buffer_to_texture(plCommandBuffer* ptCommandBuffer, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
    for(uint32_t uIndex = 0; uIndex < uRegionCount; uIndex++)
    {
        ptCommandBuffer->uCurrentStreamItem++;
        pl_sb_add(ptCommandBuffer->sbtStream);
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].eType = PL_CPU_COMMAND_BUFFER_ITEM_TYPE_COPY_BUFFER_TO_TEXTURE;
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tBufferImageCopy  = ptRegions[uIndex];
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tBufferHandle = tBufferHandle;
        ptCommandBuffer->sbtStream[ptCommandBuffer->uCurrentStreamItem].tTextureHandle = tTextureHandle;
    }
}

void
pl_graphics_destroy_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    PL_LOG_TRACE_API_F(gptLog, uLogChannelGraphics, "destroy buffer %u immediately", tHandle.uIndex);
    ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration++;
    pl_sb_push(ptDevice->sbtBufferFreeIndices, tHandle.uIndex);

    PL_FREE(ptDevice->sbtBuffersHot[tHandle.uIndex].pData);
}

void
pl_graphics_destroy_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
}

void
pl_graphics_destroy_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
}

void
pl_graphics_destroy_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    int a = 5;
}

void
pl_graphics_destroy_bind_group_layout(plDevice* ptDevice, plBindGroupLayoutHandle tHandle)
{
}

void
pl_graphics_destroy_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
}

void
pl_graphics_destroy_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
}

void
pl_graphics_destroy_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
}

void
pl_graphics_destroy_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
}

void
pl_graphics_insert_debug_label(plCommandBuffer* ptCmdBuffer, const char* pcLabel, plVec4 tColor)
{

}

void
pl_graphics_push_debug_group(plCommandBuffer* ptCmdBuffer, const char* pcLabel, plVec4 tColor)
{
}

void
pl_graphics_pop_debug_group(plCommandBuffer* ptCmdBuffer)
{
}

void
pl_graphics_intra_pass_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope, const plPassResources* ptResources)
{
}

void
pl_graphics_consumer_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope)
{
}

void
pl_graphics_producer_barrier(plCommandBuffer* ptCmdBuffer, plPipelineStageFlags tSrcStages, plPipelineStageFlags tDstStages, plBarrierScope tScope)
{
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------


static void
pl__garbage_collect(plDevice* ptDevice)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
    plFrameContext* ptCurrentFrame = pl__get_frame_resources(ptDevice);

    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_reset(ptGarbage->sbtTextures);
    pl_sb_reset(ptGarbage->sbtShaders);
    pl_sb_reset(ptGarbage->sbtComputeShaders);
    pl_sb_reset(ptGarbage->sbtRenderPasses);
    pl_sb_reset(ptGarbage->sbtRenderPassLayouts);
    pl_sb_reset(ptGarbage->sbtMemory);
    pl_sb_reset(ptGarbage->sbtBuffers);
    pl_sb_reset(ptGarbage->sbtBindGroups);
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}
