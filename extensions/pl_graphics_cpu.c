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

typedef struct _plInternalDeviceAllocatorData
{
    plDevice*                 ptDevice;
    plDeviceMemoryAllocatorI* ptAllocator;
} plInternalDeviceAllocatorData;

typedef struct _plRenderPassCommonData
{
    uint32_t uDependencyCount;
    uint32_t uColorAttachmentCount;
} plRenderPassCommonData;

typedef struct _plCpuDynamicBuffer
{
    uint32_t                 uHandle;
    // VkBuffer                 tBuffer;
    plDeviceMemoryAllocation tMemory;
    // VkDescriptorSet          tDescriptorSet;
} plCputDynamicBuffer;

typedef struct _plCpuBuffer
{
    void* pData;
} plCpuBuffer;

typedef struct _plCpuTexture
{
    // VkImage     tImage;
    // VkImageView tImageView;
    bool        bOriginalView; // so we only free if original
} plCpuTexture;

typedef struct _plCpuRenderPassLayout
{
    int a;
} plCpuRenderPassLayout;

typedef struct _plCpuRenderPass
{
    // VkFramebuffer atFrameBuffers[6];
    int a;
} plCpuRenderPass;

typedef struct _plCpuBindGroupLayout
{
    int a;
} plCpuBindGroupLayout;

typedef struct _plCpuBindGroup
{
    // VkDescriptorPool      tPool; // owning pool
    bool                  bResetable;
    // VkDescriptorSet       tDescriptorSet;
    // VkDescriptorSetLayout tDescriptorSetLayout;
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

typedef struct _plCommandBuffer
{
    plDevice*          ptDevice; // for convience
    plCommandPool*     ptPool; // parent pool
    plCommandBuffer*   ptNext; // for linked list
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
    // plCpuDynamicBuffer* sbtDynamicBuffers;
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

    plShaderHandle  tCurrentShader;
    const void*     pVerticies;
    const uint32_t* puIndexBufferData;
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
    int a;
} plSurface;

typedef struct _plSwapchain
{
    plDevice*        ptDevice; // for convience
    plSwapchainInfo  tInfo;
    // VkSemaphore      atImageAvailable[PL_MAX_FRAMES_IN_FLIGHT];
    uint32_t         uImageCount;
    plTextureHandle* sbtSwapchainTextureViews;
    uint32_t         uCurrentImageIndex; // current image to use within the swap chain

    // platform specific
    plSurface* ptSurface;
    // VkImage             atImages[8];
    // VkSurfaceFormatKHR* sbtSurfaceFormats;
} plSwapchain;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline float
pl__edge_function(plVec2 one, plVec2 two, plVec2 three)
{
    return(two.x - one.x) * (three.y - one.y) - (two.y - one.y) * (three.x - one.x);
};

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

// static void
// pl_set_pixel(plWindowBlit* ptData, plVec2 input, plVec4 tColor)
// {
//     if(input.x < 0)
//         return;
//     if(input.y < 0)
//         return;
//     if(input.x >= ptData->uWidth)
//         return;
//     if(input.y >= ptData->uHeight)
//         return;

//     int iRowOffset = ptData->uWidth * 4 * (int)input.y;
//     int iPixelStart = iRowOffset + (int)input.x * 4;

//     uint8_t* puData = (uint8_t*)ptData->puData;

//     puData[iPixelStart + 0] = (unsigned char)tColor.r;
//     puData[iPixelStart + 1] = (unsigned char)tColor.g;
//     puData[iPixelStart + 2] = (unsigned char)tColor.b;
//     puData[iPixelStart + 3] = (unsigned char)tColor.a;

// };

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
    ptCpuBuffer->pData = ptAllocation->pHostMapped;

}

void
pl_graphics_reset_dynamic_data_blocks(plDevice* ptDevice)
{
}

plDynamicDataBlock
pl_graphics_allocate_dynamic_data_block(plDevice* ptDevice)
{
    plDynamicDataBlock tBlock = {0};
    return tBlock;
}

void
pl_graphics_copy_texture_to_buffer(plCommandBuffer* ptEncoder, plTextureHandle tTextureHandle, plBufferHandle tBufferHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{

}

void
pl_graphics_copy_texture(plCommandBuffer* ptEncoder, plTextureHandle tSrcHandle, plTextureHandle tDstHandle, uint32_t uRegionCount, const plImageCopy* ptRegions)
{
}

void
pl_graphics_generate_mipmaps(plCommandBuffer* ptEncoder, plTextureHandle tTexture)
{
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
    return tHandle;
}

plBindGroupLayoutHandle
pl_graphics_create_bind_group_layout(plDevice* ptDevice, const plBindGroupLayoutDesc* ptDesc)
{
    plBindGroupLayoutHandle tHandle = pl__get_new_bind_group_layout_handle(ptDevice);
    return tHandle;
}

void
pl_graphics_update_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle, const plBindGroupUpdateData* ptData)
{
}

plTextureHandle
pl_graphics_create_texture(plDevice* ptDevice, const plTextureDesc* ptDesc, plTexture **ptTextureOut)
{
    plTextureHandle tHandle = pl__get_new_texture_handle(ptDevice);
    return tHandle;
}

void
pl_graphics_bind_texture_to_memory(plDevice* ptDevice, plTextureHandle tHandle, const plDeviceMemoryAllocation* ptAllocation)
{
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
    return ptSwap->sbtSwapchainTextureViews;
}

void
pl_graphics_begin_command_recording(plCommandBuffer* ptCommandBuffer)
{
}

void
pl_graphics_begin_render_pass(plCommandBuffer* ptCmdBuffer, const plRenderInfo* ptInfo, const plPassResources* ptResource)
{
}

void
pl_graphics_end_render_pass(plCommandBuffer* ptEncoder)
{
}

void
pl_graphics_bind_vertex_buffers(plCommandBuffer* ptEncoder, uint32_t uFirst, uint32_t uCount, const plBufferHandle* ptHandles, const size_t* pszOffsets)
{
}

void
pl_graphics_bind_vertex_buffer(plCommandBuffer* ptCommandBuffer, plBufferHandle tHandle)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    ptDevice->pVerticies = ptDevice->sbtBuffersHot[tHandle.uIndex].pData;
}

void
pl_graphics_draw(plCommandBuffer* ptEncoder, uint32_t uCount, const plDraw *atDraws)
{
}

void
pl_graphics_draw_indexed(plCommandBuffer* ptCommandBuffer, uint32_t uCount, const plDrawIndex *atDraws)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    const plShader* ptShader = &ptDevice->sbtShadersCold[ptDevice->tCurrentShader.uIndex];
    const plCpuShader* ptPipeline = &ptDevice->sbtShadersHot[ptDevice->tCurrentShader.uIndex];
    // plWindowBlit* ptBlit = ptCommandBuffer->ptPool->ptBlit;

    // calculate frame buffer size
    // const int fbWidth = ptBlit->uWidth;
    // const int fbHeight = ptBlit->uHeight;
    const int fbWidth = 0;
    const int fbHeight = 0;

    for(uint32_t uDrawIndex = 0; uDrawIndex < uCount; uDrawIndex++)
    {

        ptDevice->puIndexBufferData = ptDevice->sbtBuffersHot[atDraws[uDrawIndex].tIndexBuffer.uIndex].pData; 
    
        plVec2 vertexP = {
            .x = 0,
            .y = 0
        };  

        for(uint32_t i = 0; i < atDraws[uDrawIndex].uIndexCount; i += 3)
        {
            const uint32_t uIndex0 = ptDevice->puIndexBufferData[atDraws[uDrawIndex].uIndexStart + i];
            const uint32_t uIndex1 = ptDevice->puIndexBufferData[atDraws[uDrawIndex].uIndexStart + i + 1];
            const uint32_t uIndex2 = ptDevice->puIndexBufferData[atDraws[uDrawIndex].uIndexStart + i + 2];

            // type casting void buffer
            const char* pcVtxBuffer = (char*)ptDevice->pVerticies;

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

            // defining varying data
            plVaryingData tVaryingData0 = {0};
            plVaryingData tVaryingData1 = {0};
            plVaryingData tVaryingData2 = {0};

            // function pointer returning plVec2 containing vertex data
            // Returns vec2        // function ptr                   // built-ins    //VertexDataIn                                                                           // VaryingDataOut
            // plVec2 tOriginalVertex0 = gtData.ptPipeline->tVertexShader(tVSBuiltIns0, &pcVtxBuffer[uIndex0 * gtData.ptPipeline->tLayout.szVertexStride], &gtData.tDescriptor, &tVaryingData0);
            // plVec2 tOriginalVertex1 = gtData.ptPipeline->tVertexShader(tVSBuiltIns1, &pcVtxBuffer[uIndex1 * gtData.ptPipeline->tLayout.szVertexStride], &gtData.tDescriptor, &tVaryingData1);
            // plVec2 tOriginalVertex2 = gtData.ptPipeline->tVertexShader(tVSBuiltIns2, &pcVtxBuffer[uIndex2 * gtData.ptPipeline->tLayout.szVertexStride], &gtData.tDescriptor, &tVaryingData2);

            plVec2 tOriginalVertex0 = ptPipeline->tVertexShader(tVSBuiltIns0, &pcVtxBuffer[uIndex0 * ptShader->tDesc.atVertexBufferLayouts[0].uByteStride], &tVaryingData0);
            plVec2 tOriginalVertex1 = ptPipeline->tVertexShader(tVSBuiltIns1, &pcVtxBuffer[uIndex1 * ptShader->tDesc.atVertexBufferLayouts[0].uByteStride], &tVaryingData1);
            plVec2 tOriginalVertex2 = ptPipeline->tVertexShader(tVSBuiltIns2, &pcVtxBuffer[uIndex2 * ptShader->tDesc.atVertexBufferLayouts[0].uByteStride], &tVaryingData2);


            // frame buffer space
            plVec2 tVertex0 = tOriginalVertex0;
            plVec2 tVertex1 = tOriginalVertex1;
            plVec2 tVertex2 = tOriginalVertex2;

            tVertex0.x = fbWidth * (0.5f + 0.5f * tVertex0.x);
            tVertex1.x = fbWidth * (0.5f + 0.5f * tVertex1.x);
            tVertex2.x = fbWidth * (0.5f + 0.5f * tVertex2.x);

            tVertex0.y = fbHeight * (0.5f + 0.5f * tVertex0.y);
            tVertex1.y = fbHeight * (0.5f + 0.5f * tVertex1.y);
            tVertex2.y = fbHeight * (0.5f + 0.5f * tVertex2.y);

            // edge function for entire triangle 
            float ABC = (float)pl__edge_function(tVertex0, tVertex1, tVertex2);

            // Bounding box with clamping
            const int minX = pl_max(0, pl_min3((int)tVertex0.x, (int)tVertex1.x, (int)tVertex2.x) - 1);
            const int minY = pl_max(0, pl_min3((int)tVertex0.y, (int)tVertex1.y, (int)tVertex2.y) - 1);
            const int maxX = pl_min(fbWidth-1, pl_max3((int)tVertex0.x, (int)tVertex1.x, (int)tVertex2.x) + 1);
            const int maxY = pl_min(fbHeight-1, pl_max3((int)tVertex0.y, (int)tVertex1.y, (int)tVertex2.y) + 1);

            // Precompute edge function deltas
            const float ABa = tVertex0.y - tVertex1.y;
            const float ABb = tVertex1.x - tVertex0.x;
            const float ABc = tVertex0.x * tVertex1.y - tVertex1.x * tVertex0.y;

            const float BCa = tVertex1.y - tVertex2.y;
            const float BCb = tVertex2.x - tVertex1.x;
            const float BCc = tVertex1.x * tVertex2.y - tVertex2.x * tVertex1.y;

            const float CAa = tVertex2.y - tVertex0.y;
            const float CAb = tVertex0.x - tVertex2.x;
            const float CAc = tVertex2.x * tVertex0.y - tVertex0.x * tVertex2.y;

            // Initialize at start of row
            float ABP = ABa * minX + ABb * minY + ABc;
            float BCP = BCa * minX + BCb * minY + BCc;
            float CAP = CAa * minX + CAb * minY + CAc;

            const float invABC = 1.0f / ABC;

            for(vertexP.y = (float)minY; vertexP.y <= (float)maxY; vertexP.y++)
            {
                float rowABP = ABP;
                float rowBCP = BCP;
                float rowCAP = CAP;
            
                for(vertexP.x = (float)minX; vertexP.x <= (float)maxX; vertexP.x++)
                {
                    if(rowABP <= 0 && rowBCP <= 0 && rowCAP <= 0)
                    {
                        const float weightA = rowBCP * invABC;
                        const float weightB = rowCAP * invABC;
                        const float weightC = rowABP * invABC;
                        
                        plPixelShaderBuiltIns tBuiltIns = {
                            .gl_FragCoord.xy = vertexP
                        };

                        // Varying system
                        int varyDataOffset = 0;
                        plVaryingData blendedVaryingData = {0};

                        for(uint32_t j = 0; j < 16; j++)
                            blendedVaryingData._auOffset[j] = tVaryingData0._auOffset[j];

                        int iVaryingCount = 0;
                        for(int varyIndex = 0; varyIndex < 16; varyIndex++)
                        {
                            if(tVaryingData0.atTypes[varyIndex] == PL_VARYING_TYPE_NONE)
                                break;
                            iVaryingCount++;
                        }

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

                        // run pixel shader
                        // plVec4 tFinalColor = gtData.ptPipeline->tPixelShader(tBuiltIns, &gtData.tDescriptor, &blendedVaryingData);
                        // plVec4 tFinalColor = ptPipeline->tPixelShader(tBuiltIns, &blendedVaryingData);
                        // pl_set_pixel(ptBlit, vertexP, tFinalColor);
                    }
                    // Incrementally update edge functions for next pixel in row
                    rowABP += ABa;
                    rowBCP += BCa;
                    rowCAP += CAa;
                }
                // Incrementally update edge functions for next row
                ABP += ABb;
                BCP += BCb;
                CAP += CAb;
            }
        }
    }
}

void
pl_graphics_bind_shader(plCommandBuffer* ptCommandBuffer, plShaderHandle tHandle)
{
    plDevice* ptDevice = ptCommandBuffer->ptDevice;
    ptDevice->tCurrentShader = tHandle;
}

void
pl_graphics_bind_compute_shader(plCommandBuffer* ptEncoder, plComputeShaderHandle tHandle)
{
}

void
pl_graphics_draw_stream(plCommandBuffer* ptEncoder, uint32_t uAreaCount, plDrawArea *atAreas)
{

}

void
pl_graphics_set_depth_bias(plCommandBuffer* ptEncoder, float fDepthBiasConstantFactor, float fDepthBiasClamp, float fDepthBiasSlopeFactor)
{
}

void
pl_graphics_set_viewport(plCommandBuffer* ptEncoder, const plRenderViewport* ptViewport)
{
}

void
pl_graphics_set_scissor_region(plCommandBuffer* ptEncoder, const plScissor* ptScissor)
{
}

plDeviceMemoryAllocation
pl_graphics_allocate_memory(plDevice* ptDevice, size_t szSize, plMemoryFlags tMemoryFlags, uint32_t uTypeFilter, const char* pcName)
{
    plDeviceMemoryAllocation tAllocation = {0};
    tAllocation.ulSize = szSize;
    tAllocation.pHostMapped = malloc(szSize);
    memset(tAllocation.pHostMapped, 0, szSize);
    return tAllocation;
}

void
pl_graphics_free_memory(plDevice* ptDevice, plDeviceMemoryAllocation* ptBlock)
{
    free(ptBlock->pHostMapped);
    ptBlock->pHostMapped = NULL;
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
    // pl_sb_add(ptDevice->sbtTextureViewsHot);
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

    
    return ptDevice;
}

plSurface*
pl_graphics_create_surface(plWindow* ptWindow)
{
    plSurface* ptSurface = PL_ALLOC(sizeof(plSurface));
    memset(ptSurface, 0, sizeof(plSurface));
    return ptSurface;
}

plSwapchain*
pl_graphics_create_swapchain(plDevice* ptDevice, plSurface* ptSurface, const plSwapchainInit* ptInit)
{

    plSwapchain* ptSwap = PL_ALLOC(sizeof(plSwapchain));
    memset(ptSwap, 0, sizeof(plSwapchain));
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
    gptGraphics->uCurrentFrameIndex = (gptGraphics->uCurrentFrameIndex + 1) % gptGraphics->uFramesInFlight;
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    return true;
}

void
pl_graphics_recreate_swapchain(plSwapchain* ptSwap, const plSwapchainInit* ptInit)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);
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
    pl__cleanup_common_swapchain(ptSwap);
}

void
pl_graphics_cleanup_device(plDevice* ptDevice)
{
    pl_sb_reset(ptDevice->sbtShadersHot);
    pl__cleanup_common_device(ptDevice);
}

void
pl_graphics_begin_compute_pass(plCommandBuffer* ptCmdBuffer, const plPassResources* ptResources)
{
}

void
pl_graphics_end_compute_pass(plCommandBuffer* ptEncoder)
{
}

void
pl_graphics_dispatch(plCommandBuffer* ptEncoder, uint32_t uDispatchCount, const plDispatch *atDispatches)
{
}

void
pl_graphics_bind_compute_bind_groups(plCommandBuffer* ptEncoder, plComputeShaderHandle tHandle, uint32_t uFirst,
    uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
}

void
pl_graphics_bind_graphics_bind_groups(plCommandBuffer* ptEncoder, plShaderHandle tHandle, uint32_t uFirst, uint32_t uCount, const plBindGroupHandle *atBindGroups, uint32_t uDynamicBindingCount, const plDynamicBinding* ptDynamicBinding)
{
}

void
pl_graphics_submit_command_buffer(plCommandBuffer* ptCmdBuffer, const plSubmitInfo* ptSubmitInfo)
{
}

void
pl_graphics_wait_on_command_buffer(plCommandBuffer* ptCmdBuffer)
{
}

void
pl_graphics_return_command_buffer(plCommandBuffer* ptCmdBuffer)
{
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
    // ptPool->ptBlit = (plWindowBlit*)ptDesc;
    return ptPool;
}

void
pl_graphics_cleanup_command_pool(plCommandPool* ptPool)
{
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
    }
    ptCommandBuffer->ptPool = ptPool;
    ptCommandBuffer->ptDevice = ptPool->ptDevice;
    return ptCommandBuffer;
}

void
pl_graphics_copy_buffer(plCommandBuffer* ptEncoder, plBufferHandle tSource, plBufferHandle tDestination, uint64_t uSourceOffset, uint64_t uDestinationOffset, size_t szSize)
{
}

void
pl_graphics_copy_buffer_to_texture(plCommandBuffer* ptEncoder, plBufferHandle tBufferHandle, plTextureHandle tTextureHandle, uint32_t uRegionCount, const plBufferImageCopy* ptRegions)
{
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
