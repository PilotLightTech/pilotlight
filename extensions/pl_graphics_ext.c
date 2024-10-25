#include "pl_ds.h"
#include "pl_log.h"
#include "pl_graphics_ext.h"
#include "pl_graphics_internal.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_VULKAN_BACKEND
#include "pl_graphics_vulkan.c"
#elif PL_METAL_BACKEND
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

static plRenderPassHandle
pl_get_main_render_pass(plDevice* ptDevice)
{
    return ptDevice->tMainRenderPass;
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
    return &ptDevice->sbFrames[gptGraphics->uCurrentFrameIndex];
}

static plBuffer*
pl__get_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtBufferGenerations[tHandle.uIndex])
        return NULL;
    return &ptDevice->sbtBuffersCold[tHandle.uIndex];
}

static plTexture*
pl__get_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtTextureGenerations[tHandle.uIndex])
        return NULL;
    return &ptDevice->sbtTexturesCold[tHandle.uIndex];
}

static plBindGroup*
pl__get_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtBindGroupGenerations[tHandle.uIndex])
        return NULL;
    return &ptDevice->sbtBindGroupsCold[tHandle.uIndex];
}

static plShader*
pl__get_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtShaderGenerations[tHandle.uIndex])
        return NULL;
    return &ptDevice->sbtShadersCold[tHandle.uIndex];
}

static plSampler*
pl_get_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtSamplerGenerations[tHandle.uIndex])
        return NULL;
    return &ptDevice->sbtSamplersCold[tHandle.uIndex];
}

static plRenderPassLayout*
pl_get_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtRenderPassLayoutGenerations[tHandle.uIndex])
        return NULL;
    return &ptDevice->sbtRenderPassLayoutsCold[tHandle.uIndex];
}

static plRenderPass*
pl_get_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    if(tHandle.uGeneration != ptDevice->sbtRenderPassGenerations[tHandle.uIndex])
        return NULL;
    return &ptDevice->sbtRenderPassesCold[tHandle.uIndex];
}

static void
pl_queue_buffer_for_deletion(plDevice* ptDevice, plBufferHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtBuffers, tHandle);
    pl_sb_push(ptGarbage->sbtMemory, ptDevice->sbtBuffersCold[tHandle.uIndex].tMemoryAllocation);
    ptDevice->sbtBufferGenerations[tHandle.uIndex]++;
}

static void
pl_queue_texture_for_deletion(plDevice* ptDevice, plTextureHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtTextures, tHandle);
    pl_sb_push(ptGarbage->sbtMemory, ptDevice->sbtTexturesCold[tHandle.uIndex].tMemoryAllocation);
    ptDevice->sbtTextureGenerations[tHandle.uIndex]++;
}

static void
pl_queue_render_pass_for_deletion(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtRenderPasses, tHandle);
    ptDevice->sbtRenderPassGenerations[tHandle.uIndex]++;
}

static void
pl_queue_render_pass_layout_for_deletion(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtRenderPassLayouts, tHandle);
    ptDevice->sbtRenderPassLayoutGenerations[tHandle.uIndex]++;
}

static void
pl_queue_shader_for_deletion(plDevice* ptDevice, plShaderHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtShaders, tHandle);
    ptDevice->sbtShaderGenerations[tHandle.uIndex]++;
}

static void
pl_queue_compute_shader_for_deletion(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtComputeShaders, tHandle);
    ptDevice->sbtComputeShaderGenerations[tHandle.uIndex]++;
}

static void
pl_queue_bind_group_for_deletion(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtBindGroups, tHandle);
    ptDevice->sbtBindGroupGenerations[tHandle.uIndex]++;
}

static void
pl_queue_sampler_for_deletion(plDevice* ptDevice, plSamplerHandle tHandle)
{
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptDevice);
    pl_sb_push(ptGarbage->sbtSamplers, tHandle);
    ptDevice->sbtSamplerGenerations[tHandle.uIndex]++;
}

static void
pl_drawstream_cleanup(plDrawStream* ptStream)
{
    memset(&ptStream->_tCurrentDraw, 255, sizeof(plDrawStreamData)); 
    pl_sb_free(ptStream->_sbtStream);
}

static void
pl_drawstream_reset(plDrawStream* ptStream)
{
    memset(&ptStream->_tCurrentDraw, 255, sizeof(plDrawStreamData));
    ptStream->_tCurrentDraw.uIndexBuffer = UINT32_MAX - 1;
    ptStream->_tCurrentDraw.uDynamicBufferOffset0 = 0;
    pl_sb_reset(ptStream->_sbtStream);
}

static void
pl_drawstream_draw(plDrawStream* ptStream, plDrawStreamData tDraw)
{

    uint32_t uDirtyMask = PL_DRAW_STREAM_BIT_NONE;

    if(ptStream->_tCurrentDraw.uShaderVariant != tDraw.uShaderVariant)
    {
        ptStream->_tCurrentDraw.uShaderVariant = tDraw.uShaderVariant;
        uDirtyMask |= PL_DRAW_STREAM_BIT_SHADER;
    }

    if(ptStream->_tCurrentDraw.uDynamicBufferOffset0 != tDraw.uDynamicBufferOffset0)
    {   
        ptStream->_tCurrentDraw.uDynamicBufferOffset0 = tDraw.uDynamicBufferOffset0;
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET;
    }

    if(ptStream->_tCurrentDraw.uBindGroup0 != tDraw.uBindGroup0)
    {
        ptStream->_tCurrentDraw.uBindGroup0 = tDraw.uBindGroup0;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_0;
    }

    if(ptStream->_tCurrentDraw.uBindGroup1 != tDraw.uBindGroup1)
    {
        ptStream->_tCurrentDraw.uBindGroup1 = tDraw.uBindGroup1;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_1;
    }

    if(ptStream->_tCurrentDraw.uBindGroup2 != tDraw.uBindGroup2)
    {
        ptStream->_tCurrentDraw.uBindGroup2 = tDraw.uBindGroup2;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_2;
    }

    if(ptStream->_tCurrentDraw.uDynamicBuffer0 != tDraw.uDynamicBuffer0)
    {
        ptStream->_tCurrentDraw.uDynamicBuffer0 = tDraw.uDynamicBuffer0;
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER;
    }

    if(ptStream->_tCurrentDraw.uIndexOffset != tDraw.uIndexOffset)
    {   
        ptStream->_tCurrentDraw.uIndexOffset = tDraw.uIndexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_OFFSET;
    }

    if(ptStream->_tCurrentDraw.uVertexOffset != tDraw.uVertexOffset)
    {   
        ptStream->_tCurrentDraw.uVertexOffset = tDraw.uVertexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_OFFSET;
    }

    if(ptStream->_tCurrentDraw.uIndexBuffer != tDraw.uIndexBuffer)
    {
        ptStream->_tCurrentDraw.uIndexBuffer = tDraw.uIndexBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_BUFFER;
    }

    if(ptStream->_tCurrentDraw.uVertexBuffer != tDraw.uVertexBuffer)
    {
        ptStream->_tCurrentDraw.uVertexBuffer = tDraw.uVertexBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_BUFFER;
    }

    if(ptStream->_tCurrentDraw.uTriangleCount != tDraw.uTriangleCount)
    {
        ptStream->_tCurrentDraw.uTriangleCount = tDraw.uTriangleCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_TRIANGLES;
    }

    if(ptStream->_tCurrentDraw.uInstanceStart != tDraw.uInstanceStart)
    {
        ptStream->_tCurrentDraw.uInstanceStart = tDraw.uInstanceStart;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_START;
    }

    if(ptStream->_tCurrentDraw.uInstanceCount != tDraw.uInstanceCount)
    {
        ptStream->_tCurrentDraw.uInstanceCount = tDraw.uInstanceCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_COUNT;
    }

    pl_sb_push(ptStream->_sbtStream, uDirtyMask);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uShaderVariant);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uDynamicBufferOffset0);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uBindGroup0);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uBindGroup1);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uBindGroup2);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uDynamicBuffer0);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uIndexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uVertexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uIndexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uVertexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uTriangleCount);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_START)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uInstanceStart);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
        pl_sb_push(ptStream->_sbtStream, ptStream->_tCurrentDraw.uInstanceCount);
}

static uint32_t
pl__format_stride(plFormat tFormat)
{
    switch(tFormat)
    {
        case PL_FORMAT_D32_FLOAT_S8_UINT:  return 5;
        case PL_FORMAT_R32G32B32A32_FLOAT: return 16;
        case PL_FORMAT_R32G32_FLOAT:       return 8;
        case PL_FORMAT_R8G8B8A8_SRGB:
        case PL_FORMAT_B8G8R8A8_SRGB:
        case PL_FORMAT_B8G8R8A8_UNORM:
        case PL_FORMAT_R8G8B8A8_UNORM:     return 4;
        case PL_FORMAT_D24_UNORM_S8_UINT:
        case PL_FORMAT_D32_FLOAT:          return 1;
        
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
    pl_sb_free(ptDevice->sbtFreeDrawBindGroups);
    pl_sb_free(ptDevice->sbtShadersCold);
    pl_sb_free(ptDevice->sbtBuffersCold);
    pl_sb_free(ptDevice->sbtShaderFreeIndices);
    pl_sb_free(ptDevice->sbtBufferFreeIndices);
    pl_sb_free(ptDevice->sbtTexturesCold);
    pl_sb_free(ptDevice->sbtSamplersCold);
    pl_sb_free(ptDevice->sbtBindGroupsCold);
    pl_sb_free(ptDevice->sbtShaderGenerations);
    pl_sb_free(ptDevice->sbtBufferGenerations);
    pl_sb_free(ptDevice->sbtTextureGenerations);
    pl_sb_free(ptDevice->sbtBindGroupGenerations);
    pl_sb_free(ptDevice->sbtRenderPassesCold);
    pl_sb_free(ptDevice->sbtRenderPassGenerations);
    pl_sb_free(ptDevice->sbtTextureFreeIndices);
    pl_sb_free(ptDevice->sbtRenderPassLayoutsCold);
    pl_sb_free(ptDevice->sbtComputeShadersCold);
    pl_sb_free(ptDevice->sbtComputeShaderGenerations);
    pl_sb_free(ptDevice->sbtRenderPassLayoutGenerations);
    pl_sb_free(ptDevice->sbtSamplerGenerations);
    pl_sb_free(ptDevice->sbtBindGroupFreeIndices);
    pl_sb_free(ptDevice->sbtSemaphoreGenerations);
    pl_sb_free(ptDevice->sbtSemaphoreFreeIndices);
    pl_sb_free(ptDevice->sbtSamplerFreeIndices);
    pl_sb_free(ptDevice->sbtComputeShaderFreeIndices);

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
    }
    pl_sb_free(ptDevice->sbtGarbage);

    PL_FREE(ptDevice);
}

static plBufferHandle
pl__get_new_buffer_handle(plDevice* ptDevice)
{
    uint32_t uBufferIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtBufferFreeIndices) > 0)
        uBufferIndex = pl_sb_pop(ptDevice->sbtBufferFreeIndices);
    else
    {
        uBufferIndex = pl_sb_size(ptDevice->sbtBuffersCold);
        pl_sb_add(ptDevice->sbtBuffersCold);
        pl_sb_push(ptDevice->sbtBufferGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtBuffersHot);
    }

    plBufferHandle tHandle = {
        .uGeneration = ++ptDevice->sbtBufferGenerations[uBufferIndex],
        .uIndex = uBufferIndex
    };
    return tHandle;
}

static plTextureHandle
pl__get_new_texture_handle(plDevice* ptDevice)
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
    return tHandle;
}

static plSamplerHandle
pl__get_new_sampler_handle(plDevice* ptDevice)
{
    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtSamplerFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtSamplerFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptDevice->sbtSamplersCold);
        pl_sb_add(ptDevice->sbtSamplersCold);
        pl_sb_push(ptDevice->sbtSamplerGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtSamplersHot);
    }

    plSamplerHandle tHandle = {
        .uGeneration = ++ptDevice->sbtSamplerGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };
    return tHandle;
}

static plBindGroupHandle
pl__get_new_bind_group_handle(plDevice* ptDevice)
{
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
    return tHandle;
}

static plShaderHandle
pl__get_new_shader_handle(plDevice* ptDevice)
{
    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtShaderFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptDevice->sbtShadersCold);
        pl_sb_add(ptDevice->sbtShadersCold);
        pl_sb_push(ptDevice->sbtShaderGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtShadersHot);
    }

    plShaderHandle tHandle = {
        .uGeneration = ++ptDevice->sbtShaderGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };
    return tHandle;
}

static plComputeShaderHandle
pl__get_new_compute_shader_handle(plDevice* ptDevice)
{
    uint32_t uResourceIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtComputeShaderFreeIndices) > 0)
        uResourceIndex = pl_sb_pop(ptDevice->sbtComputeShaderFreeIndices);
    else
    {
        uResourceIndex = pl_sb_size(ptDevice->sbtComputeShadersCold);
        pl_sb_add(ptDevice->sbtComputeShadersCold);
        pl_sb_push(ptDevice->sbtComputeShaderGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtComputeShadersHot);
    }

    plComputeShaderHandle tHandle = {
        .uGeneration = ++ptDevice->sbtComputeShaderGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };
    return tHandle;
}

static plRenderPassHandle
pl__get_new_render_pass_handle(plDevice* ptDevice)
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
    return tHandle;
}

static plRenderPassLayoutHandle
pl__get_new_render_pass_layout_handle(plDevice* ptDevice)
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

    const plRenderPassLayoutHandle tHandle = {
        .uGeneration = ++ptDevice->sbtRenderPassLayoutGenerations[uResourceIndex],
        .uIndex = uResourceIndex
    };
    return tHandle;
}

static plSemaphoreHandle
pl__get_new_semaphore_handle(plDevice* ptDevice)
{
    uint32_t uIndex = UINT32_MAX;
    if(pl_sb_size(ptDevice->sbtSemaphoreFreeIndices) > 0)
        uIndex = pl_sb_pop(ptDevice->sbtSemaphoreFreeIndices);
    else
    {
        uIndex = pl_sb_size(ptDevice->sbtSemaphoresHot);
        pl_sb_push(ptDevice->sbtSemaphoreGenerations, UINT32_MAX);
        pl_sb_add(ptDevice->sbtSemaphoresHot);
    }

    plSemaphoreHandle tHandle = {
        .uGeneration = ++ptDevice->sbtSemaphoreGenerations[uIndex],
        .uIndex = uIndex
    };
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

static plRenderPassHandle
pl_get_encoder_render_pass(plRenderEncoder* ptEncoder)
{
    return ptEncoder->tRenderPassHandle;
}
static uint32_t
pl_get_render_encoder_subpass(plRenderEncoder* ptEncoder)
{
    return ptEncoder->_uCurrentSubpass;
}

static const plGraphicsI*
pl_load_graphics_api(void)
{
    static const plGraphicsI tApi = {
        .initialize                             = pl_initialize_graphics,
        .resize                                 = pl_resize,
        .get_current_frame_index                = pl_get_current_frame_index,
        .get_host_memory_in_use                 = pl_get_host_memory_in_use,
        .get_local_memory_in_use                = pl_get_local_memory_in_use,
        .get_frames_in_flight                   = pl_get_frames_in_flight,
        .begin_frame                            = pl_begin_frame,
        .create_device                          = pl__create_device,
        .enumerate_devices                      = pl_enumerate_devices,
        .cleanup_device                         = pl_cleanup_device,
        .create_swapchain                       = pl_create_swapchain,
        .create_surface                         = pl_create_surface,
        .cleanup_surface                        = pl_cleanup_surface,
        .cleanup_swapchain                      = pl_cleanup_swapchain,
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
        .draw_stream                            = pl_draw_stream,
        .draw                                   = pl_draw,
        .draw_indexed                           = pl_draw_indexed,
        .present                                = pl_present,
        .copy_buffer_to_texture                 = pl_copy_buffer_to_texture,
        .copy_texture_to_buffer                 = pl_copy_texture_to_buffer,
        .generate_mipmaps                       = pl_generate_mipmaps,
        .copy_buffer                            = pl_copy_buffer,
        .signal_semaphore                       = pl_signal_semaphore,
        .wait_semaphore                         = pl_wait_semaphore,
        .get_semaphore_value                    = pl_get_semaphore_value,
        .reset_draw_stream                      = pl_drawstream_reset,
        .add_to_stream                          = pl_drawstream_draw,
        .cleanup_draw_stream                    = pl_drawstream_cleanup,
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
        .get_main_render_pass                   = pl_get_main_render_pass,
        .get_render_pass_layout                 = pl_get_render_pass_layout,
        .create_command_pool                    = pl_create_command_pool,
        .cleanup_command_pool                   = pl_cleanup_command_pool,
        .reset_command_pool                     = pl_reset_command_pool,
        .request_command_buffer                = pl_request_command_buffer,
    };
    return &tApi;
}

static void
pl_load_graphics_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->add(PL_API_GRAPHICS, pl_load_graphics_api());
    
    if(bReload)
    {
        gptGraphics = gptDataRegistry->get_data("plGraphics");
        uLogChannelGraphics = pl_get_log_channel_id("Graphics");
    }
}

static void
pl_unload_graphics_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->remove(pl_load_graphics_api());
}