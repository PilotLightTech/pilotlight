#include "pl_ds.h"
#include "pl_graphics_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

typedef struct _plFrameGarbage
{
    plTextureHandle*          sbtTextures;
    plSamplerHandle*          sbtSamplers;
    plBufferHandle*           sbtBuffers;
    plBindGroupHandle*        sbtBindGroups;
    plShaderHandle*           sbtShaders;
    plComputeShaderHandle*    sbtComputeShaders;
    plRenderPassLayoutHandle* sbtRenderPassLayouts;
    plRenderPassHandle*       sbtRenderPasses;
    plDeviceMemoryAllocation* sbtMemory;
} plFrameGarbage;

static plFrameGarbage*
pl__get_frame_garbage(plGraphics* ptGraphics)
{
    return &ptGraphics->sbtGarbage[ptGraphics->uCurrentFrameIndex]; 
}

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

static plBuffer*
pl__get_buffer(plDevice* ptDevice, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtBufferGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtBuffersCold[tHandle.uIndex];
}

static plTexture*
pl__get_texture(plDevice* ptDevice, plTextureHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtTextureGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtTexturesCold[tHandle.uIndex];
}

static plBindGroup*
pl__get_bind_group(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtBindGroupGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtBindGroupsCold[tHandle.uIndex];
}

static plShader*
pl__get_shader(plDevice* ptDevice, plShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtShaderGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtShadersCold[tHandle.uIndex];
}

static plSampler*
pl_get_sampler(plDevice* ptDevice, plSamplerHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtSamplerGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtSamplersCold[tHandle.uIndex];
}

static plRenderPassLayout*
pl_get_render_pass_layout(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtRenderPassLayoutGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtRenderPassLayoutsCold[tHandle.uIndex];
}

static plRenderPass*
pl_get_render_pass(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    if(tHandle.uGeneration != ptGraphics->sbtRenderPassGenerations[tHandle.uIndex])
        return NULL;
    return &ptGraphics->sbtRenderPassesCold[tHandle.uIndex];
}

static void
pl_queue_buffer_for_deletion(plDevice* ptDevice, plBufferHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtBuffers, tHandle);
    pl_sb_push(ptGarbage->sbtMemory, ptGraphics->sbtBuffersCold[tHandle.uIndex].tMemoryAllocation);
    ptGraphics->sbtBufferGenerations[tHandle.uIndex]++;
}

static void
pl_queue_texture_for_deletion(plDevice* ptDevice, plTextureHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtTextures, tHandle);
    pl_sb_push(ptGarbage->sbtMemory, ptGraphics->sbtTexturesCold[tHandle.uIndex].tMemoryAllocation);
    ptGraphics->sbtTextureGenerations[tHandle.uIndex]++;
}

static void
pl_queue_render_pass_for_deletion(plDevice* ptDevice, plRenderPassHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtRenderPasses, tHandle);
    ptGraphics->sbtRenderPassGenerations[tHandle.uIndex]++;
}

static void
pl_queue_render_pass_layout_for_deletion(plDevice* ptDevice, plRenderPassLayoutHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtRenderPassLayouts, tHandle);
    ptGraphics->sbtRenderPassLayoutGenerations[tHandle.uIndex]++;
}

static void
pl_queue_shader_for_deletion(plDevice* ptDevice, plShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtShaders, tHandle);
    ptGraphics->sbtShaderGenerations[tHandle.uIndex]++;
}

static void
pl_queue_compute_shader_for_deletion(plDevice* ptDevice, plComputeShaderHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtComputeShaders, tHandle);
    ptGraphics->sbtComputeShaderGenerations[tHandle.uIndex]++;
}

static void
pl_queue_bind_group_for_deletion(plDevice* ptDevice, plBindGroupHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtBindGroups, tHandle);
    ptGraphics->sbtBindGroupGenerations[tHandle.uIndex]++;
}

static void
pl_queue_sampler_for_deletion(plDevice* ptDevice, plSamplerHandle tHandle)
{
    plGraphics* ptGraphics = ptDevice->ptGraphics;
    plFrameGarbage* ptGarbage = pl__get_frame_garbage(ptGraphics);
    pl_sb_push(ptGarbage->sbtSamplers, tHandle);
    ptGraphics->sbtSamplerGenerations[tHandle.uIndex]++;
}



//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plDrawStreamBits
{
    PL_DRAW_STREAM_BIT_NONE             = 0,
    PL_DRAW_STREAM_BIT_SHADER           = 1 << 0,
    PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET   = 1 << 1,
    PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER   = 1 << 2,
    PL_DRAW_STREAM_BIT_BINDGROUP_2      = 1 << 3,
    PL_DRAW_STREAM_BIT_BINDGROUP_1      = 1 << 4,
    PL_DRAW_STREAM_BIT_BINDGROUP_0      = 1 << 5,
    PL_DRAW_STREAM_BIT_INDEX_OFFSET     = 1 << 6,
    PL_DRAW_STREAM_BIT_VERTEX_OFFSET    = 1 << 7,
    PL_DRAW_STREAM_BIT_INDEX_BUFFER     = 1 << 8,
    PL_DRAW_STREAM_BIT_VERTEX_BUFFER    = 1 << 9,
    PL_DRAW_STREAM_BIT_TRIANGLES        = 1 << 10,
    PL_DRAW_STREAM_BIT_INSTANCE_START   = 1 << 11,
    PL_DRAW_STREAM_BIT_INSTANCE_COUNT   = 1 << 12
};

static void
pl_drawstream_cleanup(plDrawStream* ptStream)
{
    memset(&ptStream->tCurrentDraw, 255, sizeof(plStreamDraw)); 
    pl_sb_free(ptStream->sbtStream);
}

static void
pl_drawstream_reset(plDrawStream* ptStream)
{
    memset(&ptStream->tCurrentDraw, 255, sizeof(plStreamDraw));
    ptStream->tCurrentDraw.uIndexBuffer = UINT32_MAX - 1;
    ptStream->tCurrentDraw.uDynamicBufferOffset = 0;
    pl_sb_reset(ptStream->sbtStream);
}

static void
pl_drawstream_draw(plDrawStream* ptStream, plStreamDraw tDraw)
{

    uint32_t uDirtyMask = PL_DRAW_STREAM_BIT_NONE;

    if(ptStream->tCurrentDraw.uShaderVariant != tDraw.uShaderVariant)
    {
        ptStream->tCurrentDraw.uShaderVariant = tDraw.uShaderVariant;
        uDirtyMask |= PL_DRAW_STREAM_BIT_SHADER;
    }

    if(ptStream->tCurrentDraw.uDynamicBufferOffset != tDraw.uDynamicBufferOffset)
    {   
        ptStream->tCurrentDraw.uDynamicBufferOffset = tDraw.uDynamicBufferOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET;
    }

    if(ptStream->tCurrentDraw.uBindGroup0 != tDraw.uBindGroup0)
    {
        ptStream->tCurrentDraw.uBindGroup0 = tDraw.uBindGroup0;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_0;
    }

    if(ptStream->tCurrentDraw.uBindGroup1 != tDraw.uBindGroup1)
    {
        ptStream->tCurrentDraw.uBindGroup1 = tDraw.uBindGroup1;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_1;
    }

    if(ptStream->tCurrentDraw.uBindGroup2 != tDraw.uBindGroup2)
    {
        ptStream->tCurrentDraw.uBindGroup2 = tDraw.uBindGroup2;
        uDirtyMask |= PL_DRAW_STREAM_BIT_BINDGROUP_2;
    }

    if(ptStream->tCurrentDraw.uDynamicBuffer != tDraw.uDynamicBuffer)
    {
        ptStream->tCurrentDraw.uDynamicBuffer = tDraw.uDynamicBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER;
    }

    if(ptStream->tCurrentDraw.uIndexOffset != tDraw.uIndexOffset)
    {   
        ptStream->tCurrentDraw.uIndexOffset = tDraw.uIndexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_OFFSET;
    }

    if(ptStream->tCurrentDraw.uVertexOffset != tDraw.uVertexOffset)
    {   
        ptStream->tCurrentDraw.uVertexOffset = tDraw.uVertexOffset;
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_OFFSET;
    }

    if(ptStream->tCurrentDraw.uIndexBuffer != tDraw.uIndexBuffer)
    {
        ptStream->tCurrentDraw.uIndexBuffer = tDraw.uIndexBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INDEX_BUFFER;
    }

    if(ptStream->tCurrentDraw.uVertexBuffer != tDraw.uVertexBuffer)
    {
        ptStream->tCurrentDraw.uVertexBuffer = tDraw.uVertexBuffer;
        uDirtyMask |= PL_DRAW_STREAM_BIT_VERTEX_BUFFER;
    }

    if(ptStream->tCurrentDraw.uTriangleCount != tDraw.uTriangleCount)
    {
        ptStream->tCurrentDraw.uTriangleCount = tDraw.uTriangleCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_TRIANGLES;
    }

    if(ptStream->tCurrentDraw.uInstanceStart != tDraw.uInstanceStart)
    {
        ptStream->tCurrentDraw.uInstanceStart = tDraw.uInstanceStart;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_START;
    }

    if(ptStream->tCurrentDraw.uInstanceCount != tDraw.uInstanceCount)
    {
        ptStream->tCurrentDraw.uInstanceCount = tDraw.uInstanceCount;
        uDirtyMask |= PL_DRAW_STREAM_BIT_INSTANCE_COUNT;
    }

    pl_sb_push(ptStream->sbtStream, uDirtyMask);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_SHADER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uShaderVariant);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uDynamicBufferOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_0)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup0);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_1)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup1);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_BINDGROUP_2)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uBindGroup2);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_DYNAMIC_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uDynamicBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uIndexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_OFFSET)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uVertexOffset);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INDEX_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uIndexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_VERTEX_BUFFER)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uVertexBuffer);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_TRIANGLES)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uTriangleCount);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_START)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uInstanceStart);
    if(uDirtyMask & PL_DRAW_STREAM_BIT_INSTANCE_COUNT)
        pl_sb_push(ptStream->sbtStream, ptStream->tCurrentDraw.uInstanceCount);
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
pl__cleanup_common_graphics(plGraphics* ptGraphics)
{


    // cleanup per frame resources
    for(uint32_t i = 0; i < pl_sb_size(ptGraphics->sbtGarbage); i++)
    {
        plFrameGarbage* ptGarbage = &ptGraphics->sbtGarbage[i];
        pl_sb_free(ptGarbage->sbtMemory);
        pl_sb_free(ptGarbage->sbtTextures);
        pl_sb_free(ptGarbage->sbtBuffers);
        pl_sb_free(ptGarbage->sbtComputeShaders);
        pl_sb_free(ptGarbage->sbtShaders);
        pl_sb_free(ptGarbage->sbtRenderPasses);
        pl_sb_free(ptGarbage->sbtRenderPassLayouts);
        pl_sb_free(ptGarbage->sbtBindGroups);
    }

    pl_sb_free(ptGraphics->sbtGarbage);
    pl_sb_free(ptGraphics->sbtFreeDrawBindGroups);
    pl_sb_free(ptGraphics->tSwapchain.sbtSwapchainTextureViews);
    pl_sb_free(ptGraphics->sbtShadersCold);
    pl_sb_free(ptGraphics->sbtBuffersCold);
    pl_sb_free(ptGraphics->sbtShaderFreeIndices);
    pl_sb_free(ptGraphics->sbtBufferFreeIndices);
    pl_sb_free(ptGraphics->sbtTexturesCold);
    pl_sb_free(ptGraphics->sbtSamplersCold);
    pl_sb_free(ptGraphics->sbtBindGroupsCold);
    pl_sb_free(ptGraphics->sbtShaderGenerations);
    pl_sb_free(ptGraphics->sbtBufferGenerations);
    pl_sb_free(ptGraphics->sbtTextureGenerations);
    pl_sb_free(ptGraphics->sbtBindGroupGenerations);
    pl_sb_free(ptGraphics->sbtRenderPassesCold);
    pl_sb_free(ptGraphics->sbtRenderPassGenerations);
    pl_sb_free(ptGraphics->sbtTextureFreeIndices);
    pl_sb_free(ptGraphics->sbtRenderPassLayoutsCold);
    pl_sb_free(ptGraphics->sbtComputeShadersCold);
    pl_sb_free(ptGraphics->sbtComputeShaderGenerations);
    pl_sb_free(ptGraphics->sbtRenderPassLayoutGenerations);
    pl_sb_free(ptGraphics->sbtSamplerGenerations);
    pl_sb_free(ptGraphics->sbtBindGroupFreeIndices);
    pl_sb_free(ptGraphics->sbtSemaphoreGenerations);
    pl_sb_free(ptGraphics->sbtSemaphoreFreeIndices);
    pl_sb_free(ptGraphics->sbtSamplerFreeIndices);
    pl_sb_free(ptGraphics->sbtComputeShaderFreeIndices);

    PL_FREE(ptGraphics->_pInternalData);
    PL_FREE(ptGraphics->tDevice._pInternalData);
    PL_FREE(ptGraphics->tSwapchain._pInternalData);
}
