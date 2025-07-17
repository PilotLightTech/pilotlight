
#include "pl_graphics_internal.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_CPU_BACKEND
    #include "pl_graphics_cpu.c"
#elif defined(PL_VULKAN_BACKEND)
    #include "pl_graphics_vulkan.c"
#elif defined(PL_METAL_BACKEND)
    #include "pl_graphics_metal.m"
#else
#endif

static size_t
pl__get_data_type_size(plDataType tType)
{
    switch(tType)
    {
        case PL_DATA_TYPE_BOOL:   return sizeof(int);
        case PL_DATA_TYPE_BOOL2:  return 2 * sizeof(int);
        case PL_DATA_TYPE_BOOL3:  return 3 * sizeof(int);
        case PL_DATA_TYPE_BOOL4:  return 4 * sizeof(int);
        
        case PL_DATA_TYPE_FLOAT:  return sizeof(float);
        case PL_DATA_TYPE_FLOAT2: return 2 * sizeof(float);
        case PL_DATA_TYPE_FLOAT3: return 3 * sizeof(float);
        case PL_DATA_TYPE_FLOAT4: return 4 * sizeof(float);

        case PL_DATA_TYPE_UNSIGNED_BYTE:
        case PL_DATA_TYPE_BYTE:  return sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT:
        case PL_DATA_TYPE_SHORT: return sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT:
        case PL_DATA_TYPE_INT:   return sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG:
        case PL_DATA_TYPE_LONG:  return sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE2:
        case PL_DATA_TYPE_BYTE2:  return 2 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT2:
        case PL_DATA_TYPE_SHORT2: return 2 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT2:
        case PL_DATA_TYPE_INT2:   return 2 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG2:
        case PL_DATA_TYPE_LONG2:  return 2 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE3:
        case PL_DATA_TYPE_BYTE3:  return 3 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT3:
        case PL_DATA_TYPE_SHORT3: return 3 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT3:
        case PL_DATA_TYPE_INT3:   return 3 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG3:
        case PL_DATA_TYPE_LONG3:  return 3 * sizeof(uint64_t);

        case PL_DATA_TYPE_UNSIGNED_BYTE4:
        case PL_DATA_TYPE_BYTE4:  return 4 * sizeof(uint8_t);

        case PL_DATA_TYPE_UNSIGNED_SHORT4:
        case PL_DATA_TYPE_SHORT4: return 4 * sizeof(uint16_t);

        case PL_DATA_TYPE_UNSIGNED_INT4:
        case PL_DATA_TYPE_INT4:   return 4 * sizeof(uint32_t);

        case PL_DATA_TYPE_UNSIGNED_LONG4:
        case PL_DATA_TYPE_LONG4:  return 4 * sizeof(uint64_t);
    }

    PL_ASSERT(false && "Unsupported data type");
    return 0;
}

static uint32_t
pl_get_frames_in_flight(void)
{
    return gptGraphics->uFramesInFlight;
}

static uint32_t
pl_get_current_frame_index(void)
{
    return gptGraphics->uCurrentFrameIndex;
}

static size_t
pl_get_local_memory_in_use(void)
{
    return gptGraphics->szLocalMemoryInUse;
}

static size_t
pl_get_host_memory_in_use(void)
{
    return gptGraphics->szHostMemoryInUse;
}

static plFrameGarbage*
pl__get_frame_garbage(plDevice* ptDevice)
{
    return &ptDevice->sbtGarbage[gptGraphics->uCurrentFrameIndex]; 
}

static plFrameContext*
pl__get_frame_resources(plDevice* ptDevice)
{
    return &ptDevice->sbtFrames[gptGraphics->uCurrentFrameIndex];
}

static plBuffer*
pl__get_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtBuffersCold[tHandle.uIndex];
}

static plTexture*
pl__get_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtTexturesCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtTexturesCold[tHandle.uIndex];
}

static plBindGroup*
pl__get_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtBindGroupsCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtBindGroupsCold[tHandle.uIndex];
}

static plBindGroupLayout*
pl__get_bind_group_layout(plDevice* ptDevice, plBindGroupLayoutHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtBindGroupLayoutsCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtBindGroupLayoutsCold[tHandle.uIndex];
}

static plShader*
pl__get_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtShadersCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtShadersCold[tHandle.uIndex];
}

static plComputeShader*
pl__get_compute_shader(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtComputeShadersCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtComputeShadersCold[tHandle.uIndex];
}

static plSampler*
pl_get_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtSamplersCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtSamplersCold[tHandle.uIndex];
}

static plRenderPassLayout*
pl_get_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex];
}

static plRenderPass*
pl_get_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtRenderPassesCold[tHandle.uIndex]._uGeneration)
        return NULL;
    return &ptDevice->sbtRenderPassesCold[tHandle.uIndex];
}

static void
pl_queue_buffer_for_deletion(plDevice* ptDevice, plBufferHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtBuffers, tHandle);
    pl_sb_push(ptGarbage->sbtMemory, ptDevice->sbtBuffersCold[tHandle.uIndex].tMemoryAllocation);
    ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration++;
    pl_log_debug_f(gptLog, uLogChannelGraphics, "Queue buffer %u for deletion frame %llu", tHandle.uIndex, gptIO->ulFrameCount);
}

static void
pl_queue_texture_for_deletion(plDevice* ptDevice, plTextureHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtTextures, tHandle);
    if(ptDevice->sbtTexturesHot[tHandle.uIndex].bOriginalView)
    {
        pl_sb_push(ptGarbage->sbtMemory, ptDevice->sbtTexturesCold[tHandle.uIndex].tMemoryAllocation);
    }
    ptDevice->sbtTexturesCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_queue_render_pass_for_deletion(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtRenderPasses, tHandle);
    ptDevice->sbtRenderPassesCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_queue_render_pass_layout_for_deletion(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtRenderPassLayouts, tHandle);
    ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_queue_shader_for_deletion(plDevice* ptDevice, plShaderHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtShaders, tHandle);
    ptDevice->sbtShadersCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_queue_compute_shader_for_deletion(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtComputeShaders, tHandle);
    ptDevice->sbtComputeShadersCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_queue_bind_group_for_deletion(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtBindGroups, tHandle);
    ptDevice->sbtBindGroupsCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_queue_bind_group_layout_for_deletion(plDevice* ptDevice, plBindGroupLayoutHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtBindGroupLayouts, tHandle);
    ptDevice->sbtBindGroupLayoutsCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_queue_sampler_for_deletion(plDevice* ptDevice, plSamplerHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtSamplers, tHandle);
    ptDevice->sbtSamplersCold[tHandle.uIndex]._uGeneration++;
}

static void
pl_draw_stream_cleanup(plDrawStream* ptStream)
{
    memset(&ptStream->_tCurrentDraw, 255, sizeof(plDrawStreamData));
    PL_FREE(ptStream->_auStream);
    ptStream->_uStreamCapacity = 0;
    ptStream->_uStreamCount = 0;
}

static void
pl_draw_stream_reset(plDrawStream* ptStream, uint32_t uDrawCount)
{
    memset(&ptStream->_tCurrentDraw, 0, sizeof(plDrawStreamData));
    ptStream->_uStreamCount = 0;
    ptStream->_tCurrentDraw.auDynamicBuffers[0] = UINT16_MAX;
    ptStream->_tCurrentDraw.uIndexOffset = UINT32_MAX;
    ptStream->_tCurrentDraw.uVertexOffset = UINT32_MAX;
    ptStream->_tCurrentDraw.uInstanceOffset = UINT32_MAX;
    ptStream->_tCurrentDraw.uInstanceCount = UINT32_MAX;
    ptStream->_tCurrentDraw.uTriangleCount = UINT32_MAX;

    if(uDrawCount * 14 > ptStream->_uStreamCapacity)
    {
        uint32_t* auOldStream = ptStream->_auStream;
        uint32_t uNewCapacity = uDrawCount * 14;
        ptStream->_auStream = PL_ALLOC(sizeof(uint32_t) * uNewCapacity);
        memset(ptStream->_auStream, 0, sizeof(uint32_t) * uNewCapacity);
        ptStream->_uStreamCapacity = uNewCapacity;
        if(auOldStream)
        {
            PL_FREE(auOldStream);
        }
    }
}

static uint32_t
pl__format_stride(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_BC1_RGBA_UNORM:
        case PL_FORMAT_BC1_RGBA_SRGB:
        case PL_FORMAT_BC4_UNORM:
        case PL_FORMAT_BC4_SNORM:
        case PL_FORMAT_ETC2_R8G8B8_UNORM:
        case PL_FORMAT_ETC2_R8G8B8_SRGB:
        case PL_FORMAT_ETC2_R8G8B8A1_UNORM:
        case PL_FORMAT_ETC2_R8G8B8A1_SRGB:
        case PL_FORMAT_EAC_R11_UNORM:
        case PL_FORMAT_EAC_R11_SNORM:
        case PL_FORMAT_EAC_R11G11_UNORM:
        case PL_FORMAT_EAC_R11G11_SNORM:
            return 64 / (4 * 8);

        case PL_FORMAT_BC2_SRGB:
        case PL_FORMAT_BC2_UNORM:
        case PL_FORMAT_BC3_UNORM:
        case PL_FORMAT_BC3_SRGB:
        case PL_FORMAT_BC5_UNORM:
        case PL_FORMAT_BC5_SNORM:
        case PL_FORMAT_BC6H_UFLOAT:
        case PL_FORMAT_BC6H_FLOAT:
        case PL_FORMAT_BC7_UNORM:
        case PL_FORMAT_BC7_SRGB:
        case PL_FORMAT_ASTC_4x4_UNORM:
        case PL_FORMAT_ASTC_4x4_SRGB:
            return 128 / (4 * 8);


        case PL_FORMAT_ASTC_5x4_UNORM:
        case PL_FORMAT_ASTC_5x4_SRGB:
        case PL_FORMAT_ASTC_5x5_UNORM:
        case PL_FORMAT_ASTC_5x5_SRGB:
            return 128 / (5 * 8);

        case PL_FORMAT_ASTC_6x5_UNORM:
        case PL_FORMAT_ASTC_6x5_SRGB:
        case PL_FORMAT_ASTC_6x6_UNORM:
        case PL_FORMAT_ASTC_6x6_SRGB:
            return 128 / (6 * 8);

        case PL_FORMAT_ASTC_8x5_UNORM:
        case PL_FORMAT_ASTC_8x5_SRGB:
        case PL_FORMAT_ASTC_8x6_UNORM:
        case PL_FORMAT_ASTC_8x6_SRGB:
        case PL_FORMAT_ASTC_8x8_UNORM:
        case PL_FORMAT_ASTC_8x8_SRGB:
            return 128 / (8 * 8);

        case PL_FORMAT_ASTC_10x5_UNORM:
        case PL_FORMAT_ASTC_10x5_SRGB:
        case PL_FORMAT_ASTC_10x6_UNORM:
        case PL_FORMAT_ASTC_10x6_SRGB:
        case PL_FORMAT_ASTC_10x8_UNORM:
        case PL_FORMAT_ASTC_10x8_SRGB:
        case PL_FORMAT_ASTC_10x10_UNORM:
        case PL_FORMAT_ASTC_10x10_SRGB:
            return 128 / (10 * 8);
            
        case PL_FORMAT_ASTC_12x10_UNORM:
        case PL_FORMAT_ASTC_12x10_SRGB:
        case PL_FORMAT_ASTC_12x12_UNORM:
        case PL_FORMAT_ASTC_12x12_SRGB:
            return 128 / (12 * 8);

        case PL_FORMAT_R32G32B32A32_UINT:
        case PL_FORMAT_R32G32B32A32_SINT:
        case PL_FORMAT_R32G32B32A32_FLOAT:
            return 16;

        case PL_FORMAT_R32G32_UINT:
        case PL_FORMAT_R32G32_SINT:
        case PL_FORMAT_R16G16B16A16_UNORM:
        case PL_FORMAT_R16G16B16A16_SNORM:
        case PL_FORMAT_R16G16B16A16_UINT:
        case PL_FORMAT_R16G16B16A16_SINT:
        case PL_FORMAT_R16G16B16A16_FLOAT:
        case PL_FORMAT_R32G32_FLOAT:
            return 8;

        case PL_FORMAT_D32_FLOAT_S8_UINT:
            return 5;

        case PL_FORMAT_R8G8B8A8_SRGB:
        case PL_FORMAT_B8G8R8A8_SRGB:
        case PL_FORMAT_B8G8R8A8_UNORM:
        case PL_FORMAT_R8G8B8A8_UNORM:
        case PL_FORMAT_R32_UINT:
        case PL_FORMAT_R32_SINT:
        case PL_FORMAT_R32_FLOAT:
        case PL_FORMAT_R16G16_UNORM:
        case PL_FORMAT_R16G16_SNORM:
        case PL_FORMAT_R16G16_UINT:
        case PL_FORMAT_R16G16_SINT:
        case PL_FORMAT_R16G16_FLOAT:
        case PL_FORMAT_R8G8B8A8_SNORM:
        case PL_FORMAT_R8G8B8A8_UINT:
        case PL_FORMAT_R8G8B8A8_SINT:
        case PL_FORMAT_B10G10R10A2_UNORM:
        case PL_FORMAT_R10G10B10A2_UNORM:
        case PL_FORMAT_R10G10B10A2_UINT:
        case PL_FORMAT_R11G11B10_FLOAT:
        case PL_FORMAT_R9G9B9E5_FLOAT:
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D32_FLOAT:
            return 4;
 
        case PL_FORMAT_D16_UNORM_S8_UINT:
            return 3;
            
        case PL_FORMAT_R8G8_UNORM:
        case PL_FORMAT_R16_UNORM:
        case PL_FORMAT_R16_SNORM:
        case PL_FORMAT_R16_UINT:
        case PL_FORMAT_R16_SINT:
        case PL_FORMAT_R16_FLOAT:
        case PL_FORMAT_R8G8_SNORM:
        case PL_FORMAT_R8G8_UINT:
        case PL_FORMAT_R8G8_SINT:
        case PL_FORMAT_R8G8_SRGB:
        case PL_FORMAT_B5G6R5_UNORM:
        case PL_FORMAT_A1R5G5B5_UNORM:
        case PL_FORMAT_B5G5R5A1_UNORM:
        case PL_FORMAT_D16_UNORM:
            return 2;

        case PL_FORMAT_R8_UNORM:
        case PL_FORMAT_R8_SNORM:
        case PL_FORMAT_R8_UINT:
        case PL_FORMAT_R8_SINT:
        case PL_FORMAT_R8_SRGB:
        case PL_FORMAT_S8_UINT:
            return 1;
        
    }

    PL_ASSERT(false && "Unsupported format");
    return 0;
}

static void
pl__cleanup_common_graphics(void)
{

    plRenderEncoder* ptCurrentRenderEncoder = gptGraphics->ptRenderEncoderFreeList;
    while(ptCurrentRenderEncoder)
    {
        plRenderEncoder* ptNextEncoder = ptCurrentRenderEncoder->ptNext;
        PL_FREE(ptCurrentRenderEncoder);
        ptCurrentRenderEncoder = ptNextEncoder;
    }

    plBlitEncoder* ptCurrentBlitEncoder = gptGraphics->ptBlitEncoderFreeList;
    while(ptCurrentBlitEncoder)
    {
        plBlitEncoder* ptNextEncoder = ptCurrentBlitEncoder->ptNext;
        PL_FREE(ptCurrentBlitEncoder);
        ptCurrentBlitEncoder = ptNextEncoder;
    }

    plComputeEncoder* ptCurrentComputeEncoder = gptGraphics->ptComputeEncoderFreeList;
    while(ptCurrentComputeEncoder)
    {
        plComputeEncoder* ptNextEncoder = ptCurrentComputeEncoder->ptNext;
        PL_FREE(ptCurrentComputeEncoder);
        ptCurrentComputeEncoder = ptNextEncoder;
    }
    gptGraphics = NULL;
}

static void
pl__cleanup_common_swapchain(plSwapchain* ptSwapchain)
{
    pl_sb_free(ptSwapchain->sbtSwapchainTextureViews);
    PL_FREE(ptSwapchain);
}

static void
pl__cleanup_common_device(plDevice* ptDevice)
{
    pl_sb_free(ptDevice->sbtMemoryBlocks);
    pl_sb_free(ptDevice->sbtShadersCold);
    pl_sb_free(ptDevice->sbtBuffersCold);
    pl_sb_free(ptDevice->sbtShaderFreeIndices);
    pl_sb_free(ptDevice->sbtBufferFreeIndices);
    pl_sb_free(ptDevice->sbtTexturesCold);
    pl_sb_free(ptDevice->sbtSamplersCold);
    pl_sb_free(ptDevice->sbtRenderPassesCold);
    pl_sb_free(ptDevice->sbtTextureFreeIndices);
    pl_sb_free(ptDevice->sbtRenderPassLayoutsCold);
    pl_sb_free(ptDevice->sbtComputeShadersCold);
    pl_sb_free(ptDevice->sbtBindGroupFreeIndices);
    pl_sb_free(ptDevice->sbtSamplerFreeIndices);
    pl_sb_free(ptDevice->sbtComputeShaderFreeIndices);
    pl_sb_free(ptDevice->sbtRenderPassLayoutFreeIndices);
    pl_sb_free(ptDevice->sbtRenderPassFreeIndices);
    pl_sb_free(ptDevice->sbtBindGroupLayoutsCold);
    pl_sb_free(ptDevice->sbtBindGroupLayoutFreeIndices);

    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtBindGroupsCold); i++)
    {
        pl_sb_free(ptDevice->sbtBindGroupsCold[i]._sbtTextures);
    }
    pl_sb_free(ptDevice->sbtBindGroupsCold);

    plTimelineSemaphore* ptCurrentSemaphore = ptDevice->ptSemaphoreFreeList;
    while(ptCurrentSemaphore)
    {
        plTimelineSemaphore* ptNextSemaphore = ptCurrentSemaphore->ptNext;
        PL_FREE(ptCurrentSemaphore);
        ptCurrentSemaphore = ptNextSemaphore;
    }

    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptDevice->sbtGarbage); i++)
    {
        plFrameGarbage* ptGarbage = &ptDevice->sbtGarbage[i];
        pl_sb_free(ptGarbage->sbtMemory);
        pl_sb_free(ptGarbage->sbtTextures);
        pl_sb_free(ptGarbage->sbtBuffers);
        pl_sb_free(ptGarbage->sbtComputeShaders);
        pl_sb_free(ptGarbage->sbtShaders);
        pl_sb_free(ptGarbage->sbtRenderPasses);
        pl_sb_free(ptGarbage->sbtRenderPassLayouts);
        pl_sb_free(ptGarbage->sbtBindGroups);
        pl_sb_free(ptGarbage->sbtBindGroupLayouts);
    }
    pl_sb_free(ptDevice->sbtGarbage);

    PL_FREE(ptDevice);
}

static bool
pl_is_buffer_valid(plDevice* ptDevice, plBufferHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_bind_group_layout_valid(plDevice* ptDevice, plBindGroupLayoutHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtBuffersCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_sampler_valid(plDevice* ptDevice, plSamplerHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtSamplersCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_texture_valid(plDevice* ptDevice, plTextureHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtTexturesCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_bind_group_valid(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtBindGroupsCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_render_pass_valid(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtRenderPassesCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_render_pass_layout_valid(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_shader_valid(plDevice* ptDevice, plShaderHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtShadersCold[tHandle.uIndex]._uGeneration);
}

static bool
pl_is_compute_shader_valid(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    return (tHandle.uGeneration == ptDevice->sbtComputeShadersCold[tHandle.uIndex]._uGeneration);
}

static plBufferHandle
pl__get_new_buffer_handle(plDevice* ptDevice)
{
    uint16_t uBufferIndex = 0;
    if(pl_sb_size(ptDevice->sbtBufferFreeIndices) > 0)
        uBufferIndex = pl_sb_pop(ptDevice->sbtBufferFreeIndices);
    else
    {
        uBufferIndex = (uint16_t)pl_sb_size(ptDevice->sbtBuffersCold);
        pl_sb_add(ptDevice->sbtBuffersCold);
        pl_sb_back(ptDevice->sbtBuffersCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtBuffersHot);
    }

    plBufferHandle tHandle = {
        .uGeneration = ++ptDevice->sbtBuffersCold[uBufferIndex]._uGeneration,
        .uIndex = uBufferIndex
    };
    pl_log_trace_f(gptLog, uLogChannelGraphics, "create buffer %u", tHandle.uIndex);
    return tHandle;
}

static plTextureHandle
pl__get_new_texture_handle(plDevice* ptDevice)
{
    uint16_t uTextureIndex = 0;
    if(pl_sb_size(ptDevice->sbtTextureFreeIndices) > 0)
        uTextureIndex = pl_sb_pop(ptDevice->sbtTextureFreeIndices);
    else
    {
        uTextureIndex = (uint16_t)pl_sb_size(ptDevice->sbtTexturesCold);
        pl_sb_add(ptDevice->sbtTexturesCold);
        pl_sb_back(ptDevice->sbtTexturesCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtTexturesHot);
    }

    plTextureHandle tHandle = {
        .uGeneration = ++ptDevice->sbtTexturesCold[uTextureIndex]._uGeneration,
        .uIndex = uTextureIndex
    };
    pl_log_trace_f(gptLog, uLogChannelGraphics, "create texture %u", tHandle.uIndex);
    return tHandle;
}

static plSamplerHandle
pl__get_new_sampler_handle(plDevice* ptDevice)
{
    uint16_t uResourceIndex = 0;
    if(pl_sb_size(ptDevice->sbtSamplerFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtSamplerFreeIndices);
    else
    {
        uResourceIndex = (uint16_t)pl_sb_size(ptDevice->sbtSamplersCold);
        pl_sb_add(ptDevice->sbtSamplersCold);
        pl_sb_back(ptDevice->sbtSamplersCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtSamplersHot);
    }

    plSamplerHandle tHandle = {
        .uGeneration = ++ptDevice->sbtSamplersCold[uResourceIndex]._uGeneration,
        .uIndex = uResourceIndex
    };
    pl_log_trace_f(gptLog, uLogChannelGraphics, "create sampler %u", tHandle.uIndex);
    return tHandle;
}

static plBindGroupHandle
pl__get_new_bind_group_handle(plDevice* ptDevice)
{
    uint16_t uBindGroupIndex = 0;
    if(pl_sb_size(ptDevice->sbtBindGroupFreeIndices) > 0)
        uBindGroupIndex = pl_sb_pop(ptDevice->sbtBindGroupFreeIndices);
    else
    {
        uBindGroupIndex = (uint16_t)pl_sb_size(ptDevice->sbtBindGroupsCold);
        pl_sb_add(ptDevice->sbtBindGroupsCold);
        pl_sb_back(ptDevice->sbtBindGroupsCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtBindGroupsHot);
    }

    plBindGroupHandle tHandle = {
        .uGeneration = ++ptDevice->sbtBindGroupsCold[uBindGroupIndex]._uGeneration,
        .uIndex = uBindGroupIndex
    };
    return tHandle;
}

static plBindGroupLayoutHandle
pl__get_new_bind_group_layout_handle(plDevice* ptDevice)
{
    uint16_t uBindGroupIndex = 0;
    if(pl_sb_size(ptDevice->sbtBindGroupLayoutFreeIndices) > 0)
        uBindGroupIndex = pl_sb_pop(ptDevice->sbtBindGroupLayoutFreeIndices);
    else
    {
        uBindGroupIndex = (uint16_t)pl_sb_size(ptDevice->sbtBindGroupLayoutsCold);
        pl_sb_add(ptDevice->sbtBindGroupLayoutsCold);
        pl_sb_back(ptDevice->sbtBindGroupLayoutsCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtBindGroupLayoutsHot);
    }

    plBindGroupLayoutHandle tHandle = {
        .uGeneration = ++ptDevice->sbtBindGroupLayoutsCold[uBindGroupIndex]._uGeneration,
        .uIndex      = uBindGroupIndex
    };
    return tHandle;
}

static plShaderHandle
pl__get_new_shader_handle(plDevice* ptDevice)
{
    uint16_t uResourceIndex = 0;
    if(pl_sb_size(ptDevice->sbtShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtShaderFreeIndices);
    else
    {
        uResourceIndex = (uint16_t)pl_sb_size(ptDevice->sbtShadersCold);
        pl_sb_add(ptDevice->sbtShadersCold);
        pl_sb_back(ptDevice->sbtShadersCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtShadersHot);
    }

    plShaderHandle tHandle = {
        .uGeneration = ++ptDevice->sbtShadersCold[uResourceIndex]._uGeneration,
        .uIndex = uResourceIndex
    };
    pl_log_trace_f(gptLog, uLogChannelGraphics, "create shader %u", tHandle.uIndex);
    return tHandle;
}

static plComputeShaderHandle
pl__get_new_compute_shader_handle(plDevice* ptDevice)
{
    uint16_t uResourceIndex = 0;
    if(pl_sb_size(ptDevice->sbtComputeShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtComputeShaderFreeIndices);
    else
    {
        uResourceIndex = (uint16_t)pl_sb_size(ptDevice->sbtComputeShadersCold);
        pl_sb_add(ptDevice->sbtComputeShadersCold);
        pl_sb_back(ptDevice->sbtComputeShadersCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtComputeShadersHot);
    }

    plComputeShaderHandle tHandle = {
        .uGeneration = ++ptDevice->sbtComputeShadersCold[uResourceIndex]._uGeneration,
        .uIndex = uResourceIndex
    };
    pl_log_trace_f(gptLog, uLogChannelGraphics, "create compute shader %u", tHandle.uIndex);
    return tHandle;
}

static plRenderPassHandle
pl__get_new_render_pass_handle(plDevice* ptDevice)
{
    uint16_t uResourceIndex = 0;
    if(pl_sb_size(ptDevice->sbtRenderPassFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtRenderPassFreeIndices);
    else
    {
        uResourceIndex = (uint16_t)pl_sb_size(ptDevice->sbtRenderPassesCold);
        pl_sb_add(ptDevice->sbtRenderPassesCold);
        pl_sb_back(ptDevice->sbtRenderPassesCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtRenderPassesHot);
    }

    plRenderPassHandle tHandle = {
        .uGeneration = ++ptDevice->sbtRenderPassesCold[uResourceIndex]._uGeneration,
        .uIndex = uResourceIndex
    };
    pl_log_trace_f(gptLog, uLogChannelGraphics, "create render pass %u", tHandle.uIndex);
    return tHandle;
}

static plRenderPassLayoutHandle
pl__get_new_render_pass_layout_handle(plDevice* ptDevice)
{
    uint16_t uResourceIndex = 0;
    if(pl_sb_size(ptDevice->sbtRenderPassLayoutFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtRenderPassLayoutFreeIndices);
    else
    {
        uResourceIndex = (uint16_t)pl_sb_size(ptDevice->sbtRenderPassLayoutsCold);
        pl_sb_add(ptDevice->sbtRenderPassLayoutsCold);
        pl_sb_back(ptDevice->sbtRenderPassLayoutsCold)._uGeneration = UINT16_MAX;
        pl_sb_add(ptDevice->sbtRenderPassLayoutsHot);
    }

    const plRenderPassLayoutHandle tHandle = {
        .uGeneration = ++ptDevice->sbtRenderPassLayoutsCold[uResourceIndex]._uGeneration,
        .uIndex = uResourceIndex
    };
    pl_log_trace_f(gptLog, uLogChannelGraphics, "create render pass layout %u", tHandle.uIndex);
    return tHandle;
}

static plRenderEncoder*
pl__get_new_render_encoder(void)
{
    plRenderEncoder* ptEncoder = gptGraphics->ptRenderEncoderFreeList;
    if(ptEncoder)
    {
        gptGraphics->ptRenderEncoderFreeList = ptEncoder->ptNext;
    }
    else
    {
        ptEncoder = PL_ALLOC(sizeof(plRenderEncoder));
        memset(ptEncoder, 0, sizeof(plRenderEncoder));
    }
    return ptEncoder;
}

static plComputeEncoder*
pl__get_new_compute_encoder(void)
{
    plComputeEncoder* ptEncoder = gptGraphics->ptComputeEncoderFreeList;
    if(ptEncoder)
    {
        gptGraphics->ptComputeEncoderFreeList = ptEncoder->ptNext;
    }
    else
    {
        ptEncoder = PL_ALLOC(sizeof(plComputeEncoder));
        memset(ptEncoder, 0, sizeof(plComputeEncoder));
    }
    return ptEncoder;
}

static plBlitEncoder*
pl__get_new_blit_encoder(void)
{
    plBlitEncoder* ptEncoder = gptGraphics->ptBlitEncoderFreeList;
    if(ptEncoder)
    {
        gptGraphics->ptBlitEncoderFreeList = ptEncoder->ptNext;
    }
    else
    {
        ptEncoder = PL_ALLOC(sizeof(plBlitEncoder));
        memset(ptEncoder, 0, sizeof(plBlitEncoder));
    }
    return ptEncoder;
}

static plTimelineSemaphore*
pl__get_new_semaphore(plDevice* ptDevice)
{
    plTimelineSemaphore* ptSemaphore = ptDevice->ptSemaphoreFreeList;
    if(ptSemaphore)
    {
        ptDevice->ptSemaphoreFreeList = ptSemaphore->ptNext;
    }
    else
    {
        ptSemaphore = PL_ALLOC(sizeof(plTimelineSemaphore));
        memset(ptSemaphore, 0, sizeof(plTimelineSemaphore));
    }
    ptSemaphore->ptDevice = ptDevice;
    ptSemaphore->ptNext = NULL;
    return ptSemaphore;
}

static void
pl__return_render_encoder(plRenderEncoder* ptEncoder)
{
    ptEncoder->ptNext = gptGraphics->ptRenderEncoderFreeList;
    gptGraphics->ptRenderEncoderFreeList = ptEncoder;
}

static void
pl__return_compute_encoder(plComputeEncoder* ptEncoder)
{
    ptEncoder->ptNext = gptGraphics->ptComputeEncoderFreeList;
    gptGraphics->ptComputeEncoderFreeList = ptEncoder;
}

static void
pl__return_blit_encoder(plBlitEncoder* ptEncoder)
{
    ptEncoder->ptNext = gptGraphics->ptBlitEncoderFreeList;
    gptGraphics->ptBlitEncoderFreeList = ptEncoder;
}

static void
pl__return_semaphore(plDevice* ptDevice, plTimelineSemaphore* ptSemaphore)
{
    ptSemaphore->ptNext = ptDevice->ptSemaphoreFreeList;
    ptDevice->ptSemaphoreFreeList = ptSemaphore;
}

static plRenderPassHandle
pl_get_encoder_render_pass(plRenderEncoder* ptEncoder)
{
    return ptEncoder->tRenderPassHandle;
}
static uint32_t
pl_get_render_encoder_subpass(plRenderEncoder* ptEncoder)
{
    return ptEncoder->uCurrentSubpass;
}

static plSwapchainInfo
pl_get_swapchain_info(plSwapchain* ptSwap)
{
    return ptSwap->tInfo;
}

static const plDeviceMemoryAllocation*
pl_get_gfx_allocations(plDevice* ptDevice, uint32_t* puSizeOut)
{
    if(puSizeOut)
    {
        *puSizeOut = pl_sb_size(ptDevice->sbtMemoryBlocks);
    }
    return ptDevice->sbtMemoryBlocks;
}

const plDeviceInfo*
pl_get_device_info(plDevice* ptDevice)
{
    return &ptDevice->tInfo;
}

plCommandBuffer*
pl_get_encoder_command_buffer(plRenderEncoder* ptEncoder)
{
    return ptEncoder->ptCommandBuffer;
}

plCommandBuffer*
pl_get_compute_encoder_command_buffer(plComputeEncoder* ptEncoder)
{
    return ptEncoder->ptCommandBuffer;
}

plCommandBuffer*
pl_get_blit_encoder_command_buffer(plBlitEncoder* ptEncoder)
{
    return ptEncoder->ptCommandBuffer;
}

PL_EXPORT void
pl_load_graphics_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plGraphicsI tApi = {
        .initialize                             = pl_initialize_graphics,
        .get_backend                            = pl_get_backend,
        .get_backend_string                     = pl_get_backend_string,
        .set_depth_bias                         = pl_set_depth_bias,
        .recreate_swapchain                     = pl_recreate_swapchain,
        .get_swapchain_info                     = pl_get_swapchain_info,
        .get_current_frame_index                = pl_get_current_frame_index,
        .get_host_memory_in_use                 = pl_get_host_memory_in_use,
        .get_local_memory_in_use                = pl_get_local_memory_in_use,
        .get_frames_in_flight                   = pl_get_frames_in_flight,
        .begin_frame                            = pl_begin_frame,
        .create_device                          = pl_create_device,
        .get_device_info                        = pl_get_device_info,
        .enumerate_devices                      = pl_enumerate_devices,
        .cleanup_device                         = pl_cleanup_device,
        .acquire_swapchain_image                = pl_acquire_swapchain_image,
        .get_swapchain_images                   = pl_get_swapchain_images,
        .create_swapchain                       = pl_create_swapchain,
        .create_surface                         = pl_create_surface,
        .cleanup_surface                        = pl_cleanup_surface,
        .cleanup_swapchain                      = pl_cleanup_swapchain,
        .cleanup_semaphore                      = pl_cleanup_semaphore,
        .dispatch                               = pl_dispatch,
        .bind_compute_bind_groups               = pl_bind_compute_bind_groups,
        .bind_graphics_bind_groups              = pl_bind_graphics_bind_groups,
        .set_scissor_region                     = pl_set_scissor_region,
        .set_viewport                           = pl_set_viewport,
        .bind_vertex_buffer                     = pl_bind_vertex_buffer,
        .bind_shader                            = pl_bind_shader,
        .bind_compute_shader                    = pl_bind_compute_shader,
        .cleanup                                = pl_cleanup_graphics,
        .begin_command_recording                = pl_begin_command_recording,
        .end_command_recording                  = pl_end_command_recording,
        .submit_command_buffer                  = pl_submit_command_buffer,
        .wait_on_command_buffer                 = pl_wait_on_command_buffer,
        .return_command_buffer                  = pl_return_command_buffer,
        .reset_command_buffer                   = pl_reset_command_buffer,
        .next_subpass                           = pl_next_subpass,
        .begin_render_pass                      = pl_begin_render_pass,
        .get_encoder_render_pass                = pl_get_encoder_render_pass,
        .get_render_encoder_subpass             = pl_get_render_encoder_subpass,
        .end_render_pass                        = pl_end_render_pass,
        .begin_compute_pass                     = pl_begin_compute_pass,
        .end_compute_pass                       = pl_end_compute_pass,
        .begin_blit_pass                        = pl_begin_blit_pass,
        .end_blit_pass                          = pl_end_blit_pass,
        .set_texture_usage                      = pl_set_texture_usage,
        .draw_stream                            = pl_draw_stream,
        .draw                                   = pl_draw,
        .draw_indexed                           = pl_draw_indexed,
        .present                                = pl_present,
        .copy_buffer_to_texture                 = pl_copy_buffer_to_texture,
        .copy_texture_to_buffer                 = pl_copy_texture_to_buffer,
        .copy_texture                           = pl_copy_texture,
        .generate_mipmaps                       = pl_generate_mipmaps,
        .copy_buffer                            = pl_copy_buffer,
        .signal_semaphore                       = pl_signal_semaphore,
        .wait_semaphore                         = pl_wait_semaphore,
        .get_semaphore_value                    = pl_get_semaphore_value,
        .reset_draw_stream                      = pl_draw_stream_reset,
        .cleanup_draw_stream                    = pl_draw_stream_cleanup,
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
        .create_bind_group_layout               = pl_create_bind_group_layout,
        .destroy_bind_group_layout              = pl_destroy_bind_group_layout,
        .queue_bind_group_layout_for_deletion   = pl_queue_bind_group_layout_for_deletion,
        .get_bind_group_layout                  = pl__get_bind_group_layout,
        .is_bind_group_layout_valid             = pl_is_bind_group_layout_valid,
        .update_bind_group                      = pl_update_bind_group,
        .allocate_dynamic_data_block            = pl_allocate_dynamic_data_block,
        .queue_buffer_for_deletion              = pl_queue_buffer_for_deletion,
        .queue_texture_for_deletion             = pl_queue_texture_for_deletion,
        .queue_bind_group_for_deletion          = pl_queue_bind_group_for_deletion,
        .queue_shader_for_deletion              = pl_queue_shader_for_deletion,
        .queue_compute_shader_for_deletion      = pl_queue_compute_shader_for_deletion,
        .queue_render_pass_for_deletion         = pl_queue_render_pass_for_deletion,
        .queue_render_pass_layout_for_deletion  = pl_queue_render_pass_layout_for_deletion,
        .queue_sampler_for_deletion             = pl_queue_sampler_for_deletion,
        .destroy_bind_group                     = pl_destroy_bind_group,
        .destroy_buffer                         = pl_destroy_buffer,
        .destroy_texture                        = pl_destroy_texture,
        .destroy_shader                         = pl_destroy_shader,
        .destroy_compute_shader                 = pl_destroy_compute_shader,
        .destroy_render_pass                    = pl_destroy_render_pass,
        .destroy_render_pass_layout             = pl_destroy_render_pass_layout,
        .destroy_sampler                        = pl_destroy_sampler,
        .update_render_pass_attachments         = pl_update_render_pass_attachments,
        .get_buffer                             = pl__get_buffer,
        .get_texture                            = pl__get_texture,
        .get_bind_group                         = pl__get_bind_group,
        .get_shader                             = pl__get_shader,
        .get_compute_shader                     = pl__get_compute_shader,
        .allocate_memory                        = pl_allocate_memory,
        .free_memory                            = pl_free_memory,
        .get_allocations                        = pl_get_gfx_allocations,
        .flush_device                           = pl_flush_device,
        .bind_buffer_to_memory                  = pl_bind_buffer_to_memory,
        .bind_texture_to_memory                 = pl_bind_texture_to_memory,
        .get_sampler                            = pl_get_sampler,
        .get_render_pass                        = pl_get_render_pass,
        .get_render_pass_layout                 = pl_get_render_pass_layout,
        .create_command_pool                    = pl_create_command_pool,
        .cleanup_command_pool                   = pl_cleanup_command_pool,
        .reset_command_pool                     = pl_reset_command_pool,
        .request_command_buffer                 = pl_request_command_buffer,
        .create_bind_group_pool                 = pl_create_bind_group_pool,
        .cleanup_bind_group_pool                = pl_cleanup_bind_group_pool,
        .reset_bind_group_pool                  = pl_reset_bind_group_pool,
        .pipeline_barrier_blit                  = pl_pipeline_barrier_blit,
        .pipeline_barrier_render                = pl_pipeline_barrier_render,
        .pipeline_barrier_compute               = pl_pipeline_barrier_compute,
        .is_buffer_valid                        = pl_is_buffer_valid,
        .is_sampler_valid                       = pl_is_sampler_valid,
        .is_texture_valid                       = pl_is_texture_valid,
        .is_bind_group_valid                    = pl_is_bind_group_valid,
        .is_render_pass_valid                   = pl_is_render_pass_valid,
        .is_render_pass_layout_valid            = pl_is_render_pass_layout_valid,
        .is_shader_valid                        = pl_is_shader_valid,
        .is_compute_shader_valid                = pl_is_compute_shader_valid,
        .get_compute_encoder_command_buffer     = pl_get_compute_encoder_command_buffer,
        .get_blit_encoder_command_buffer        = pl_get_blit_encoder_command_buffer,
        .get_encoder_command_buffer             = pl_get_encoder_command_buffer,
        .invalidate_memory                      = pl_gfx_invalidate_memory,
        .flush_memory                           = pl_gfx_flush_memory,

        #if defined(PL_GRAPHICS_EXPOSE_VULKAN) && defined(PL_VULKAN_BACKEND)
        .get_vulkan_instance        = pl_get_vulkan_instance,
        .get_vulkan_api_version     = pl_get_vulkan_api_version,
        .get_vulkan_device          = pl_get_vulkan_device,
        .get_vulkan_physical_device = pl_get_vulkan_physical_device,
        .get_vulkan_queue           = pl_get_vulkan_queue,
        .get_vulkan_queue_family    = pl_get_vulkan_queue_family,
        .get_vulkan_render_pass     = pl_get_vulkan_render_pass,
        .get_vulkan_descriptor_pool = pl_get_vulkan_descriptor_pool,
        .get_vulkan_sample_count    = pl_get_vulkan_sample_count,
        .get_vulkan_command_buffer  = pl_get_vulkan_command_buffer,
        .get_vulkan_image_view      = pl_get_vulkan_image_view,
        .get_vulkan_sampler         = pl_get_vulkan_sampler,
        .get_vulkan_descriptor_set  = pl_get_vulkan_descriptor_set,
        .get_vulkan_allocation_callbacks = pl_get_vulkan_allocation_callbacks,
        #endif

        #if defined(PL_GRAPHICS_EXPOSE_METAL) && defined(PL_METAL_BACKEND)
        .get_metal_device                 = pl_get_metal_device,
        .get_metal_render_pass_descriptor = pl_get_metal_render_pass_descriptor,
        .get_metal_command_buffer         = pl_get_metal_command_buffer,
        .get_metal_command_encoder        = pl_get_metal_command_encoder,
        .get_metal_texture                = pl_get_metal_texture,
        .get_metal_bind_group_texture     = pl_get_metal_bind_group_texture,
        #endif
    };
    pl_set_api(ptApiRegistry, plGraphicsI, &tApi);

    gptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptThreads      = pl_get_api_latest(ptApiRegistry, plThreadsI);
    gptProfile      = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog          = pl_get_api_latest(ptApiRegistry, plLogI);
    gptMemory       = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptIOI          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptIO           = gptIOI->get_io();

    if(bReload)
    {
        gptGraphics = gptDataRegistry->get_data("plGraphics");
        uLogChannelGraphics = gptLog->get_channel_id("Graphics");
    }
}

PL_EXPORT void
pl_unload_graphics_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plGraphicsI* ptApi = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    ptApiRegistry->remove_api(ptApi);
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif