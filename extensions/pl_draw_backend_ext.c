#include <float.h>
#include "pl.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS

// extensions
#include "pl_graphics_ext.h"
#include "pl_shader_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_draw_ext.h"
#include "pl_stats_ext.h"

#include "pl_ext.inc"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plPipelineEntry
{
    plRenderPassHandle tRenderPass;
    uint32_t           uMSAASampleCount;
    plShaderHandle     tRegularPipeline;
    plShaderHandle     tSecondaryPipeline;
    plDrawFlags        tFlags;
    uint32_t           uSubpassIndex;
} plPipelineEntry;

typedef struct _plBufferInfo
{
    plBufferHandle tVertexBuffer;
    uint32_t       uVertexBufferSize;
    uint32_t       uVertexBufferOffset;
} plBufferInfo;

typedef struct _plDrawBackendContext
{
    plDevice*            ptDevice;
    plTempAllocator      tTempAllocator;
    plSamplerHandle      tFontSampler;
    plBindGroupHandle    tFontSamplerBindGroup;
    plPipelineEntry*     sbt3dPipelineEntries;
    plPipelineEntry*     sbt2dPipelineEntries;
    plBindGroupPool*     ptBindGroupPool;

    // shared resources
    plBufferHandle atIndexBuffer[PL_MAX_FRAMES_IN_FLIGHT];
    uint32_t       auIndexBufferSize[PL_MAX_FRAMES_IN_FLIGHT];
    uint32_t       auIndexBufferOffset[PL_MAX_FRAMES_IN_FLIGHT];

    plBufferInfo atBufferInfo[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferInfo at3DBufferInfo[PL_MAX_FRAMES_IN_FLIGHT];
    plBufferInfo atLineBufferInfo[PL_MAX_FRAMES_IN_FLIGHT];

    // dynamic buffer system
    plDynamicDataBlock tCurrentDynamicDataBlock;
} plDrawBackendContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plDrawBackendContext* gptDrawBackendCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plBufferHandle         pl__create_staging_buffer(const plBufferDesc*, const char* pcName, uint32_t uIdentifier);
static const plPipelineEntry* pl__get_3d_pipeline              (plRenderPassHandle, uint32_t uMSAASampleCount, plDrawFlags, uint32_t uSubpassIndex);
static const plPipelineEntry* pl__get_2d_pipeline              (plRenderPassHandle, uint32_t uMSAASampleCount, uint32_t uSubpassIndex);
static plBindGroupHandle      pl_create_bind_group_for_texture(plTextureHandle);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl_initialize_draw_backend(plDevice* ptDevice)
{
    gptDrawBackendCtx->ptDevice = ptDevice;

    pl_sb_reserve(gptDrawBackendCtx->sbt3dPipelineEntries, 32);
    pl_sb_reserve(gptDrawBackendCtx->sbt2dPipelineEntries, 32);

    // create initial buffers
    const plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_STAGING,
        .szByteSize  = 4096,
        .pcDebugName = "Draw Ext Idx Buffer"
    };

    const plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STAGING,
        .szByteSize  = 4096,
        .pcDebugName = "Draw Ext Vtx Buffer"
    }; 

    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptDrawBackendCtx->auIndexBufferSize[i] = 4096;
        gptDrawBackendCtx->atBufferInfo[i].uVertexBufferSize = 4096;
        gptDrawBackendCtx->at3DBufferInfo[i].uVertexBufferSize = 4096;
        gptDrawBackendCtx->atLineBufferInfo[i].uVertexBufferSize = 4096;
        gptDrawBackendCtx->atIndexBuffer[i] = pl__create_staging_buffer(&tIndexBufferDesc, "draw idx buffer", i);
        gptDrawBackendCtx->atBufferInfo[i].tVertexBuffer= pl__create_staging_buffer(&tVertexBufferDesc, "draw vtx buffer", i);
        gptDrawBackendCtx->at3DBufferInfo[i].tVertexBuffer= pl__create_staging_buffer(&tVertexBufferDesc, "3d draw vtx buffer", i);
        gptDrawBackendCtx->atLineBufferInfo[i].tVertexBuffer= pl__create_staging_buffer(&tVertexBufferDesc, "3d line draw vtx buffer", i);
    }

    // 2d
    const plSamplerDesc tSamplerDesc = {
        .tMagFilter      = PL_FILTER_LINEAR,
        .tMinFilter      = PL_FILTER_LINEAR,
        .fMinMip         = -1000.0f,
        .fMaxMip         = 1000.0f,
        .fMaxAnisotropy  = 1.0f,
        .tVAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tUAddressMode   = PL_ADDRESS_MODE_WRAP,
        .tMipmapMode     = PL_MIPMAP_MODE_LINEAR,
        .pcDebugName     = "2D Drawing Font Sampler"
    };
    gptDrawBackendCtx->tFontSampler = gptGfx->create_sampler(ptDevice, &tSamplerDesc);

    const plBindGroupPoolDesc tPoolDesc = {
        .tFlags = PL_BIND_GROUP_POOL_FLAGS_INDIVIDUAL_RESET,
        .szSamplerBindings = 10,
        .szSampledTextureBindings = 10000
    };
    gptDrawBackendCtx->ptBindGroupPool = gptGfx->create_bind_group_pool(ptDevice, &tPoolDesc);

    const plBindGroupLayout tSamplerBindGroupLayout = {
        .atSamplerBindings = {
            {.uSlot =  0, .tStages = PL_STAGE_PIXEL}
        }
    };
    const plBindGroupDesc tSamplerBindGroupDesc = {
        .ptPool = gptDrawBackendCtx->ptBindGroupPool,
        .ptLayout = &tSamplerBindGroupLayout,
        .pcDebugName = "font sampler bind group"
    };
    gptDrawBackendCtx->tFontSamplerBindGroup = gptGfx->create_bind_group(ptDevice, &tSamplerBindGroupDesc);
    const plBindGroupUpdateSamplerData atSamplerData[] = {
        { .uSlot = 0, .tSampler = gptDrawBackendCtx->tFontSampler}
    };

    plBindGroupUpdateData tBGData0 = {
        .uSamplerCount = 1,
        .atSamplerBindings = atSamplerData,
    };
    gptGfx->update_bind_group(ptDevice, gptDrawBackendCtx->tFontSamplerBindGroup, &tBGData0);
}

static void
pl_cleanup_draw_backend(void)
{
    plDevice* ptDevice = gptDrawBackendCtx->ptDevice;
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptGfx->destroy_buffer(ptDevice, gptDrawBackendCtx->atBufferInfo[i].tVertexBuffer);
        gptGfx->destroy_buffer(ptDevice, gptDrawBackendCtx->at3DBufferInfo[i].tVertexBuffer);
        gptGfx->destroy_buffer(ptDevice, gptDrawBackendCtx->atLineBufferInfo[i].tVertexBuffer);
        gptGfx->destroy_buffer(ptDevice, gptDrawBackendCtx->atIndexBuffer[i]);  
        gptGfx->destroy_buffer(ptDevice, gptDrawBackendCtx->atIndexBuffer[i]);  
    }

    gptGfx->cleanup_bind_group_pool(gptDrawBackendCtx->ptBindGroupPool);

    pl_sb_free(gptDrawBackendCtx->sbt3dPipelineEntries);
    pl_sb_free(gptDrawBackendCtx->sbt2dPipelineEntries);
    pl_temp_allocator_free(&gptDrawBackendCtx->tTempAllocator);

    gptDraw->cleanup();
}

static void
pl_new_draw_frame(void)
{

    static double* pd2dPipelineCount = NULL;
    static double* pd3dPipelineCount = NULL;

    if(!pd2dPipelineCount)
        pd2dPipelineCount = gptStats->get_counter("Draw 2D Pipelines");

    if(!pd3dPipelineCount)
        pd3dPipelineCount = gptStats->get_counter("Draw 3D Pipelines");

    *pd2dPipelineCount = pl_sb_size(gptDrawBackendCtx->sbt2dPipelineEntries);
    *pd3dPipelineCount = pl_sb_size(gptDrawBackendCtx->sbt3dPipelineEntries);

    gptDraw->new_frame();
    gptDrawBackendCtx->tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(gptDrawBackendCtx->ptDevice);

    // reset buffer offsets
    for(uint32_t i = 0; i < gptGfx->get_frames_in_flight(); i++)
    {
        gptDrawBackendCtx->atBufferInfo[i].uVertexBufferOffset = 0;
        gptDrawBackendCtx->at3DBufferInfo[i].uVertexBufferOffset = 0;
        gptDrawBackendCtx->atLineBufferInfo[i].uVertexBufferOffset = 0;
        gptDrawBackendCtx->auIndexBufferOffset[i] = 0;
    }
}

static bool
pl_build_font_atlas_backend(plCommandBuffer* ptCommandBuffer, plFontAtlas* ptAtlas)
{

    gptDraw->prepare_font_atlas(ptAtlas);

    // create texture
    const plTextureDesc tFontTextureDesc = {
        .tDimensions   = {ptAtlas->tAtlasSize.x, ptAtlas->tAtlasSize.y, 1},
        .tFormat       = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers       = 1,
        .uMips         = 1,
        .tType         = PL_TEXTURE_TYPE_2D,
        .tUsage        = PL_TEXTURE_USAGE_SAMPLED,
        .pcDebugName   = "2D Drawing Font Atlas"
    };

    plDevice* ptDevice = gptDrawBackendCtx->ptDevice;

    plTexture* ptTexture = NULL;
    plTextureHandle tTexture = gptGfx->create_texture(ptDevice, &tFontTextureDesc, &ptTexture);
    uint64_t ulData = (uint64_t)tTexture.uData;
    ptAtlas->ptUserData = (void*)ulData;
    
    const plDeviceMemoryAllocation tAllocation = gptGfx->allocate_memory(ptDevice,
        ptTexture->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU,
        ptTexture->tMemoryRequirements.uMemoryTypeBits,
        "font texture memory");

    gptGfx->bind_texture_to_memory(ptDevice, tTexture, &tAllocation);

    const plBufferDesc tBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_STAGING,
        .szByteSize  = (size_t)(ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y * 4),
        .pcDebugName = "font staging buffer"
    };
    plBufferHandle tStagingBuffer = pl__create_staging_buffer(&tBufferDesc, "font staging buffer", 0);
    plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, tStagingBuffer);
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptAtlas->pucPixelsAsRGBA32, tBufferDesc.szByteSize);

    // begin recording
    gptGfx->begin_command_recording(ptCommandBuffer, NULL);
    
    // begin blit pass, copy texture, end pass
    plBlitEncoder* ptEncoder = gptGfx->begin_blit_pass(ptCommandBuffer);

    gptGfx->set_texture_usage(ptEncoder, tTexture, PL_TEXTURE_USAGE_SAMPLED, 0);

    const plBufferImageCopy tBufferImageCopy = {
        .uImageWidth = (uint32_t)ptAtlas->tAtlasSize.x,
        .uImageHeight = (uint32_t)ptAtlas->tAtlasSize.y,
        .uImageDepth = 1,
        .uLayerCount = 1
    };

    gptGfx->copy_buffer_to_texture(ptEncoder, tStagingBuffer, tTexture, 1, &tBufferImageCopy);
    gptGfx->end_blit_pass(ptEncoder);

    // finish recording
    gptGfx->end_command_recording(ptCommandBuffer);

    // submit command buffer
    gptGfx->submit_command_buffer(ptCommandBuffer, NULL);
    gptGfx->wait_on_command_buffer(ptCommandBuffer);

    ptAtlas->tTexture = pl_create_bind_group_for_texture(tTexture).uData;

    gptGfx->destroy_buffer(ptDevice, tStagingBuffer);
    return true;
}

static void
pl_cleanup_font_atlas_backend(plFontAtlas* ptAtlas)
{
    if(ptAtlas == NULL)
        ptAtlas = gptDraw->get_current_font_atlas();

    uint64_t ulData = (uint64_t)ptAtlas->ptUserData;
    plTextureHandle tTexture = {.uData = (uint32_t)ulData};
    gptGfx->destroy_texture(gptDrawBackendCtx->ptDevice, tTexture);

    gptDraw->cleanup_font_atlas(ptAtlas);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plBufferHandle
pl__create_staging_buffer(const plBufferDesc* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = gptDrawBackendCtx->ptDevice;

    // create buffer
    plBuffer* ptBuffer = NULL;
    const plBufferHandle tHandle = gptGfx->create_buffer(ptDevice, ptDesc, &ptBuffer);
    pl_temp_allocator_reset(&gptDrawBackendCtx->tTempAllocator);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = gptGfx->allocate_memory(ptDevice,
        ptBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_GPU_CPU,
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        pl_temp_allocator_sprintf(&gptDrawBackendCtx->tTempAllocator, "%s: %u", pcName, uIdentifier));

    // bind memory
    gptGfx->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    return tHandle;
}

static const plPipelineEntry*
pl__get_3d_pipeline(plRenderPassHandle tRenderPass, uint32_t uMSAASampleCount, plDrawFlags tFlags, uint32_t uSubpassIndex)
{
    // check if pipeline exists
    for(uint32_t i = 0; i < pl_sb_size(gptDrawBackendCtx->sbt3dPipelineEntries); i++)
    {
        const plPipelineEntry* ptEntry = &gptDrawBackendCtx->sbt3dPipelineEntries[i];
        if(ptEntry->tRenderPass.uIndex == tRenderPass.uIndex && ptEntry->uMSAASampleCount == uMSAASampleCount && ptEntry->tFlags == tFlags && ptEntry->uSubpassIndex == uSubpassIndex)
        {
            return ptEntry;
        }
    }

    plDevice* ptDevice = gptDrawBackendCtx->ptDevice;

    pl_sb_add(gptDrawBackendCtx->sbt3dPipelineEntries);
    plPipelineEntry* ptEntry = &gptDrawBackendCtx->sbt3dPipelineEntries[pl_sb_size(gptDrawBackendCtx->sbt3dPipelineEntries) - 1];
    ptEntry->tFlags = tFlags;
    ptEntry->tRenderPass = tRenderPass;
    ptEntry->uMSAASampleCount = uMSAASampleCount;
    ptEntry->uSubpassIndex = uSubpassIndex;

    uint64_t ulCullMode = PL_CULL_MODE_NONE;
    if(tFlags & PL_DRAW_FLAG_CULL_FRONT)
        ulCullMode |= PL_CULL_MODE_CULL_FRONT;
    if(tFlags & PL_DRAW_FLAG_CULL_BACK)
        ulCullMode |= PL_CULL_MODE_CULL_BACK;

    {
        const plShaderDesc t3DShaderDesc = {
            .tPixelShader = gptShader->load_glsl("../shaders/draw_3d.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("../shaders/draw_3d.vert", "main", NULL, NULL),
            .tGraphicsState = {
                .ulDepthWriteEnabled  = tFlags & PL_DRAW_FLAG_DEPTH_WRITE ? 1 : 0,
                .ulDepthMode          = tFlags & PL_DRAW_FLAG_DEPTH_TEST ? PL_COMPARE_MODE_LESS : PL_COMPARE_MODE_ALWAYS,
                .ulCullMode           = ulCullMode,
                .ulWireframe          = 0,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 4,
                    .atAttributes = {
                        {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                        {.uByteOffset = sizeof(float) * 3, .tFormat = PL_FORMAT_R32_UINT},
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .tColorOp        = PL_BLEND_OP_ADD,
                    .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .tAlphaOp        = PL_BLEND_OP_ADD
                }
            },
            .tRenderPassLayout = gptGfx->get_render_pass(ptDevice, tRenderPass)->tDesc.tLayout,
            .uSubpassIndex = uSubpassIndex,
            .tMSAASampleCount = uMSAASampleCount
        };
        ptEntry->tRegularPipeline = gptGfx->create_shader(ptDevice, &t3DShaderDesc);
        pl_temp_allocator_reset(&gptDrawBackendCtx->tTempAllocator);
    }

    {
        const plShaderDesc t3DLineShaderDesc = {
            .tPixelShader = gptShader->load_glsl("../shaders/draw_3d.frag", "main", NULL, NULL),
            .tVertexShader = gptShader->load_glsl("../shaders/draw_3d_line.vert", "main", NULL, NULL),
            .tGraphicsState = {
                .ulDepthWriteEnabled  = tFlags & PL_DRAW_FLAG_DEPTH_WRITE,
                .ulDepthMode          = tFlags & PL_DRAW_FLAG_DEPTH_TEST ? PL_COMPARE_MODE_LESS : PL_COMPARE_MODE_ALWAYS,
                .ulCullMode           = ulCullMode,
                .ulWireframe          = 0,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            },
            .atVertexBufferLayouts = {
                {
                    .uByteStride = sizeof(float) * 10,
                    .atAttributes = {
                        {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                        {.uByteOffset = sizeof(float) * 3, .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                        {.uByteOffset = sizeof(float) * 6, .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                        {.uByteOffset = sizeof(float) * 9, .tFormat = PL_FORMAT_R32_UINT},
                    }
                }
            },
            .atBlendStates = {
                {
                    .bBlendEnabled   = true,
                    .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .tColorOp        = PL_BLEND_OP_ADD,
                    .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                    .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .tAlphaOp        = PL_BLEND_OP_ADD
                }
            },
            .tRenderPassLayout = gptGfx->get_render_pass(ptDevice, tRenderPass)->tDesc.tLayout,
            .uSubpassIndex = uSubpassIndex,
            .tMSAASampleCount = uMSAASampleCount
        };
        ptEntry->tSecondaryPipeline = gptGfx->create_shader(ptDevice, &t3DLineShaderDesc);
        pl_temp_allocator_reset(&gptDrawBackendCtx->tTempAllocator);
    }
    return ptEntry;
}

static const plPipelineEntry*
pl__get_2d_pipeline(plRenderPassHandle tRenderPass, uint32_t uMSAASampleCount, uint32_t uSubpassIndex)
{
    // check if pipeline exists
    for(uint32_t i = 0; i < pl_sb_size(gptDrawBackendCtx->sbt2dPipelineEntries); i++)
    {
        const plPipelineEntry* ptEntry = &gptDrawBackendCtx->sbt2dPipelineEntries[i];
        if(ptEntry->tRenderPass.uIndex == tRenderPass.uIndex && ptEntry->uMSAASampleCount == uMSAASampleCount && ptEntry->uSubpassIndex == uSubpassIndex)
        {
            return ptEntry;
        }
    }

    plDevice* ptDevice = gptDrawBackendCtx->ptDevice;

    pl_sb_add(gptDrawBackendCtx->sbt2dPipelineEntries);
    plPipelineEntry* ptEntry = &gptDrawBackendCtx->sbt2dPipelineEntries[pl_sb_size(gptDrawBackendCtx->sbt2dPipelineEntries) - 1];
    ptEntry->tFlags = 0;
    ptEntry->tRenderPass = tRenderPass;
    ptEntry->uMSAASampleCount = uMSAASampleCount;
    ptEntry->uSubpassIndex = uSubpassIndex;

    const plShaderDesc tRegularShaderDesc = {
        .tPixelShader  = gptShader->load_glsl("../shaders/draw_2d.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/draw_2d.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 5,
                .atAttributes = {
                    {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32_FLOAT},
                    {.uByteOffset = sizeof(float) * 2, .tFormat = PL_FORMAT_R32G32_FLOAT},
                    {.uByteOffset = sizeof(float) * 4, .tFormat = PL_FORMAT_R32_UINT},
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled   = true,
                .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tColorOp        = PL_BLEND_OP_ADD,
                .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tAlphaOp        = PL_BLEND_OP_ADD
            }
        },
        .tRenderPassLayout = gptGfx->get_render_pass(ptDevice, tRenderPass)->tDesc.tLayout,
        .uSubpassIndex = uSubpassIndex,
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    {.uSlot =  0, .tStages = PL_STAGE_PIXEL}
                }
            },
            {
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        },
        .tMSAASampleCount = uMSAASampleCount
    };
    ptEntry->tRegularPipeline = gptGfx->create_shader(ptDevice, &tRegularShaderDesc);
    pl_temp_allocator_reset(&gptDrawBackendCtx->tTempAllocator);

    const plShaderDesc tSecondaryShaderDesc = {
        .tPixelShader  = gptShader->load_glsl("../shaders/draw_2d_sdf.frag", "main", NULL, NULL),
        .tVertexShader = gptShader->load_glsl("../shaders/draw_2d.vert", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulDepthMode          = PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 5,
                .atAttributes = {
                    {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32_FLOAT},
                    {.uByteOffset = sizeof(float) * 2, .tFormat = PL_FORMAT_R32G32_FLOAT},
                    {.uByteOffset = sizeof(float) * 4, .tFormat = PL_FORMAT_R32_UINT},
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled   = true,
                .tSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tColorOp        = PL_BLEND_OP_ADD,
                .tSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA,
                .tDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .tAlphaOp        = PL_BLEND_OP_ADD
            }
        },
        .tRenderPassLayout = gptGfx->get_render_pass(ptDevice, tRenderPass)->tDesc.tLayout,
        .uSubpassIndex = uSubpassIndex,
        .atBindGroupLayouts = {
            {
                .atSamplerBindings = {
                    {.uSlot =  0, .tStages = PL_STAGE_PIXEL}
                }
            },
            {
                .atTextureBindings = { 
                    {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
                }
            }
        },
        .tMSAASampleCount = uMSAASampleCount
    }; 
    ptEntry->tSecondaryPipeline = gptGfx->create_shader(ptDevice, &tSecondaryShaderDesc);
    pl_temp_allocator_reset(&gptDrawBackendCtx->tTempAllocator);
    return ptEntry;
}

static plBindGroupHandle
pl_create_bind_group_for_texture(plTextureHandle tTexture)
{
    const plBindGroupLayout tDrawingBindGroup = {
        .atTextureBindings = { 
            {.uSlot = 0, .tStages = PL_STAGE_PIXEL, .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED}
        }
    };
    const plBindGroupDesc tSamplerBindGroupDesc = {
        .ptPool = gptDrawBackendCtx->ptBindGroupPool,
        .ptLayout = &tDrawingBindGroup,
        .pcDebugName = "draw texture bind group"
    };
    plBindGroupHandle tBindGroup = gptGfx->create_bind_group(gptDrawBackendCtx->ptDevice, &tSamplerBindGroupDesc);

    const plBindGroupUpdateTextureData atBGTextureData[] = {
        {
            .tTexture = tTexture,
            .uSlot    = 0,
            .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED
        }
    };
    const plBindGroupUpdateData tBGData = {
        .uTextureCount = 1,
        .atTextureBindings = atBGTextureData
    };

    gptGfx->update_bind_group(gptDrawBackendCtx->ptDevice, tBindGroup, &tBGData);

    return tBindGroup;
}

static void
pl_submit_2d_drawlist(plDrawList2D* ptDrawlist, plRenderEncoder* ptEncoder, float fWidth, float fHeight, uint32_t uMSAASampleCount)
{

    gptDraw->prepare_2d_drawlist(ptDrawlist);

    if(pl_sb_size(ptDrawlist->sbtVertexBuffer) == 0u)
        return;

    plDevice* ptDevice = gptDrawBackendCtx->ptDevice;

    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // ensure gpu vertex buffer size is adequate
    const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex) * pl_sb_size(ptDrawlist->sbtVertexBuffer);

    plBufferInfo* ptBufferInfo = &gptDrawBackendCtx->atBufferInfo[uFrameIdx];

    // space left in vertex buffer
    const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexBufferSize - ptBufferInfo->uVertexBufferOffset;

    // grow buffer if not enough room
    if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
    {

        gptGfx->queue_buffer_for_deletion(ptDevice, ptBufferInfo->tVertexBuffer);

        const plBufferDesc tBufferDesc = {
            .tUsage     = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STAGING,
            .szByteSize = pl_max(ptBufferInfo->uVertexBufferSize * 2, uVtxBufSzNeeded + uAvailableVertexBufferSpace)
        };
        ptBufferInfo->uVertexBufferSize = (uint32_t)tBufferDesc.szByteSize;

        ptBufferInfo->tVertexBuffer = pl__create_staging_buffer(&tBufferDesc, "draw vtx buffer", uFrameIdx);
    }

    // vertex GPU data transfer
    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptBufferInfo->tVertexBuffer);
    char* pucMappedVertexBufferLocation = ptVertexBuffer->tMemoryAllocation.pHostMapped;
    memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtVertexBuffer, sizeof(plDrawVertex) * pl_sb_size(ptDrawlist->sbtVertexBuffer));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // ensure gpu index buffer size is adequate
    const uint32_t uIdxBufSzNeeded = ptDrawlist->uIndexBufferByteSize;

    // space left in index buffer
    const uint32_t uAvailableIndexBufferSpace = gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] - gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx];

    if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
    {
        gptGfx->queue_buffer_for_deletion(ptDevice, gptDrawBackendCtx->atIndexBuffer[uFrameIdx]);

        const plBufferDesc tBufferDesc = {
            .tUsage     = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_STAGING,
            .szByteSize = pl_max(gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] * 2, uIdxBufSzNeeded + uAvailableIndexBufferSpace)
        };
        gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] = (uint32_t)tBufferDesc.szByteSize;

        gptDrawBackendCtx->atIndexBuffer[uFrameIdx] = pl__create_staging_buffer(&tBufferDesc, "draw idx buffer", uFrameIdx);
        gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] = 0;
    }

    // index GPU data transfer
    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, gptDrawBackendCtx->atIndexBuffer[uFrameIdx]);
    char* pucMappedIndexBufferLocation = ptIndexBuffer->tMemoryAllocation.pHostMapped;
    memcpy(&pucMappedIndexBufferLocation[gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx]], ptDrawlist->sbuIndexBuffer, uIdxBufSzNeeded);

    const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex);
    const int32_t iIndexOffset = gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] / sizeof(uint32_t);

    const plPipelineEntry* ptEntry = pl__get_2d_pipeline(gptGfx->get_encoder_render_pass(ptEncoder), uMSAASampleCount, gptGfx->get_render_encoder_subpass(ptEncoder));

    // const plVec2 tClipScale = gptIOI->get_io()->tMainFramebufferScale;
    
    // const plVec2 tClipScale = {1.0f, 1.0f};
    // const plVec2 tClipScale = ptCtx->tFrameBufferScale;
    const float fScale[] = { 2.0f / fWidth, 2.0f / fHeight};
    const float fTranslate[] = {-1.0f, -1.0f};
    plShaderHandle tCurrentShader = ptEntry->tRegularPipeline;

    typedef struct _plDrawDynamicData
    {
        plVec2 uScale;
        plVec2 uTranslate;
    } plDrawDynamicData;

    plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &gptDrawBackendCtx->tCurrentDynamicDataBlock);

    plDrawDynamicData* ptDynamicData = (plDrawDynamicData*)tDynamicBinding.pcData;
    ptDynamicData->uScale.x = 2.0f / fWidth;
    ptDynamicData->uScale.y = 2.0f / fHeight;
    ptDynamicData->uTranslate.x = -1.0f;
    ptDynamicData->uTranslate.y = -1.0f;

    bool bSdf = false;

    plRenderViewport tViewport = {
        .fWidth  = fWidth,
        .fHeight = fHeight,
        .fMaxDepth = 1.0f
    };

    gptGfx->set_viewport(ptEncoder, &tViewport);
    gptGfx->bind_vertex_buffer(ptEncoder, ptBufferInfo->tVertexBuffer);
    gptGfx->bind_shader(ptEncoder, tCurrentShader);

    const uint32_t uCmdCount = pl_sb_size(ptDrawlist->sbtDrawCommands);
    for(uint32_t i = 0u; i < uCmdCount; i++)
    {
        plDrawCommand cmd = ptDrawlist->sbtDrawCommands[i];

        if(cmd.bSdf && !bSdf)
        {
            gptGfx->bind_shader(ptEncoder, ptEntry->tSecondaryPipeline);
            tCurrentShader = ptEntry->tSecondaryPipeline;
            bSdf = true;
        }
        else if(!cmd.bSdf && bSdf)
        {
            gptGfx->bind_shader(ptEncoder, ptEntry->tRegularPipeline);
            tCurrentShader = ptEntry->tRegularPipeline;
            bSdf = false;
        }

        if(pl_rect_width(&cmd.tClip) == 0)
        {
            const plScissor tScissor = {
                .uWidth = (uint32_t)(fWidth),
                .uHeight = (uint32_t)(fHeight),
            };
            gptGfx->set_scissor_region(ptEncoder, &tScissor);
        }
        else
        {

            // cmd.tClip.tMin.x = tClipScale.x * cmd.tClip.tMin.x;
            // cmd.tClip.tMax.x = tClipScale.x * cmd.tClip.tMax.x;
            // cmd.tClip.tMin.y = tClipScale.y * cmd.tClip.tMin.y;
            // cmd.tClip.tMax.y = tClipScale.y * cmd.tClip.tMax.y;

            // clamp to viewport
            if (cmd.tClip.tMin.x < 0.0f)   { cmd.tClip.tMin.x = 0.0f; }
            if (cmd.tClip.tMin.y < 0.0f)   { cmd.tClip.tMin.y = 0.0f; }
            if (cmd.tClip.tMax.x > fWidth)  { cmd.tClip.tMax.x = (float)fWidth; }
            if (cmd.tClip.tMax.y > fHeight) { cmd.tClip.tMax.y = (float)fHeight; }
            if (cmd.tClip.tMax.x <= cmd.tClip.tMin.x || cmd.tClip.tMax.y <= cmd.tClip.tMin.y)
                continue;

            const plScissor tScissor = {
                .iOffsetX  = (uint32_t) (cmd.tClip.tMin.x < 0 ? 0 : cmd.tClip.tMin.x),
                .iOffsetY  = (uint32_t) (cmd.tClip.tMin.y < 0 ? 0 : cmd.tClip.tMin.y),
                .uWidth    = (uint32_t)pl_rect_width(&cmd.tClip),
                .uHeight   = (uint32_t)pl_rect_height(&cmd.tClip)
            };
            gptGfx->set_scissor_region(ptEncoder, &tScissor);
        }

        plBindGroupHandle tTexture = {.uData = cmd.tTextureId};
        plBindGroupHandle atBindGroups[] = {
            gptDrawBackendCtx->tFontSamplerBindGroup,
            tTexture
        };

        gptGfx->bind_graphics_bind_groups(ptEncoder, tCurrentShader, 0, 2, atBindGroups, 1, &tDynamicBinding);

        const plDrawIndex tDraw = {
            .tIndexBuffer   = gptDrawBackendCtx->atIndexBuffer[uFrameIdx],
            .uIndexCount    = cmd.uElementCount,
            .uIndexStart    = cmd.uIndexOffset + iIndexOffset,
            .uInstance      = 0,
            .uInstanceCount = 1,
            .uVertexStart   = iVertexOffset
        };
        gptGfx->draw_indexed(ptEncoder, 1, &tDraw);
    }

    // bump vertex & index buffer offset
    ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
    gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] += uIdxBufSzNeeded;
}

static void
pl_submit_3d_drawlist(plDrawList3D* ptDrawlist, plRenderEncoder* ptEncoder, float fWidth, float fHeight, const plMat4* ptMVP, plDrawFlags tFlags, uint32_t uMSAASampleCount)
{
    plDevice* ptDevice = gptDrawBackendCtx->ptDevice;
    const uint32_t uFrameIdx = gptGfx->get_current_frame_index();

    const plPipelineEntry* ptEntry = pl__get_3d_pipeline(gptGfx->get_encoder_render_pass(ptEncoder), uMSAASampleCount, tFlags, gptGfx->get_render_encoder_subpass(ptEncoder));

    const float fAspectRatio = fWidth / fHeight;

    // regular 3D
    if(pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) > 0)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);

        plBufferInfo* ptBufferInfo = &gptDrawBackendCtx->at3DBufferInfo[uFrameIdx];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexBufferSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
        {

            gptGfx->queue_buffer_for_deletion(ptDevice, ptBufferInfo->tVertexBuffer);

            const plBufferDesc tBufferDesc = {
                .tUsage     = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STAGING,
                .szByteSize = pl_max(ptBufferInfo->uVertexBufferSize * 2, uVtxBufSzNeeded + uAvailableVertexBufferSpace)
            };
            ptBufferInfo->uVertexBufferSize = (uint32_t)tBufferDesc.szByteSize;

            ptBufferInfo->tVertexBuffer = pl__create_staging_buffer(&tBufferDesc, "3d draw vtx buffer", uFrameIdx);
        }

        // vertex GPU data transfer
        plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptBufferInfo->tVertexBuffer);
        char* pucMappedVertexBufferLocation = ptVertexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtSolidVertexBuffer, sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] - gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx];

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
        {
            gptGfx->queue_buffer_for_deletion(gptDrawBackendCtx->ptDevice, gptDrawBackendCtx->atIndexBuffer[uFrameIdx]);

            const plBufferDesc tBufferDesc = {
                .tUsage     = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_STAGING,
                .szByteSize = pl_max(gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] * 2, uIdxBufSzNeeded + uAvailableIndexBufferSpace)
            };
            gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] = (uint32_t)tBufferDesc.szByteSize;

            gptDrawBackendCtx->atIndexBuffer[uFrameIdx] = pl__create_staging_buffer(&tBufferDesc, "3d draw idx buffer", uFrameIdx);
            gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] = 0;
        }

        // index GPU data transfer
        plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, gptDrawBackendCtx->atIndexBuffer[uFrameIdx]);
        char* pucMappedIndexBufferLocation = ptIndexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedIndexBufferLocation[gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx]], ptDrawlist->sbtSolidIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer));

        plDynamicBinding tSolidDynamicData = pl_allocate_dynamic_data(gptGfx, gptDrawBackendCtx->ptDevice, &gptDrawBackendCtx->tCurrentDynamicDataBlock);
        plMat4* ptSolidDynamicData = (plMat4*)tSolidDynamicData.pcData;
        *ptSolidDynamicData = *ptMVP;

        gptGfx->bind_vertex_buffer(ptEncoder, ptBufferInfo->tVertexBuffer);
        gptGfx->bind_shader(ptEncoder, ptEntry->tRegularPipeline);
        gptGfx->bind_graphics_bind_groups(ptEncoder, ptEntry->tRegularPipeline, 0, 0, NULL, 1, &tSolidDynamicData);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DSolid);
        const int32_t iIndexOffset = gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] / sizeof(uint32_t);

        const plDrawIndex tDrawIndex = {
            .tIndexBuffer = gptDrawBackendCtx->atIndexBuffer[uFrameIdx],
            .uIndexCount = pl_sb_size(ptDrawlist->sbtSolidIndexBuffer),
            .uIndexStart = iIndexOffset,
            .uInstance = 0,
            .uInstanceCount = 1,
            .uVertexStart = iVertexOffset
        };

        gptGfx->draw_indexed(ptEncoder, 1, &tDrawIndex);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] += uIdxBufSzNeeded;
    }

    // 3D lines
    if(pl_sb_size(ptDrawlist->sbtLineVertexBuffer) > 0u)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer);

        plBufferInfo* ptBufferInfo = &gptDrawBackendCtx->atLineBufferInfo[uFrameIdx];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexBufferSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
        {
            gptGfx->queue_buffer_for_deletion(ptDevice, ptBufferInfo->tVertexBuffer);

            const plBufferDesc tBufferDesc = {
                .tUsage     = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STAGING,
                .szByteSize = pl_max(ptBufferInfo->uVertexBufferSize * 2, uVtxBufSzNeeded + uAvailableVertexBufferSpace)
            };
            ptBufferInfo->uVertexBufferSize = (uint32_t)tBufferDesc.szByteSize;

            ptBufferInfo->tVertexBuffer = pl__create_staging_buffer(&tBufferDesc, "draw vtx buffer", uFrameIdx);
        }

        // vertex GPU data transfer
        plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptBufferInfo->tVertexBuffer);
        char* pucMappedVertexBufferLocation = ptVertexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtLineVertexBuffer, sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] - gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx];

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
        {

            gptGfx->queue_buffer_for_deletion(ptDevice, gptDrawBackendCtx->atIndexBuffer[uFrameIdx]);

            const plBufferDesc tBufferDesc = {
                .tUsage     = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_STAGING,
                .szByteSize = pl_max(gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] * 2, uIdxBufSzNeeded + uAvailableIndexBufferSpace)
            };
            gptDrawBackendCtx->auIndexBufferSize[uFrameIdx] = (uint32_t)tBufferDesc.szByteSize;

            gptDrawBackendCtx->atIndexBuffer[uFrameIdx] = pl__create_staging_buffer(&tBufferDesc, "draw idx buffer", uFrameIdx);
            gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] = 0;
        }

        // index GPU data transfer
        plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, gptDrawBackendCtx->atIndexBuffer[uFrameIdx]);
        char* pucMappedIndexBufferLocation = ptIndexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedIndexBufferLocation[gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx]], ptDrawlist->sbtLineIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer));
        
        typedef struct _plLineDynamiceData
        {
            plMat4 tMVP;
            float fAspect;
            int   padding[3];
        } plLineDynamiceData;

        plDynamicBinding tLineDynamicData = pl_allocate_dynamic_data(gptGfx, gptDrawBackendCtx->ptDevice, &gptDrawBackendCtx->tCurrentDynamicDataBlock);
        plLineDynamiceData* ptLineDynamicData = (plLineDynamiceData*)tLineDynamicData.pcData;
        ptLineDynamicData->tMVP = *ptMVP;
        ptLineDynamicData->fAspect = fAspectRatio;

        gptGfx->bind_vertex_buffer(ptEncoder, ptBufferInfo->tVertexBuffer);
        gptGfx->bind_shader(ptEncoder, ptEntry->tSecondaryPipeline);
        gptGfx->bind_graphics_bind_groups(ptEncoder, ptEntry->tSecondaryPipeline, 0, 0, NULL, 1, &tLineDynamicData);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DLine);
        const int32_t iIndexOffset = gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] / sizeof(uint32_t);

        const plDrawIndex tDrawIndex = {
            .tIndexBuffer = gptDrawBackendCtx->atIndexBuffer[uFrameIdx],
            .uIndexCount = pl_sb_size(ptDrawlist->sbtLineIndexBuffer),
            .uIndexStart = iIndexOffset,
            .uInstance = 0,
            .uInstanceCount = 1,
            .uVertexStart = iVertexOffset
        };

        gptGfx->draw_indexed(ptEncoder, 1, &tDrawIndex);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        gptDrawBackendCtx->auIndexBufferOffset[uFrameIdx] += uIdxBufSzNeeded;
    }

    const uint32_t uTextCount = pl_sb_size(ptDrawlist->sbtTextEntries);
    for(uint32_t i = 0; i < uTextCount; i++)
    {
        const plDraw3DText* ptText = &ptDrawlist->sbtTextEntries[i];
        plVec4 tPos = pl_mul_mat4_vec4(ptMVP, (plVec4){.xyz = ptText->tP, .w = 1});
        tPos = pl_div_vec4_scalarf(tPos, tPos.w);
        if(!(tPos.z < 0.0f || tPos.z > 1.0f))
        {
            tPos.x = fWidth * 0.5f * (1.0f + tPos.x);
            tPos.y = fHeight * 0.5f * (1.0f + tPos.y);
            pl_add_text_ex(ptDrawlist->ptLayer,
                (plVec2){roundf(tPos.x + 0.5f), roundf(tPos.y + 0.5f)},
                ptText->acText,
                (plDrawTextOptions){
                    .fSize = ptText->fSize,
                    .ptFont = ptText->ptFont,
                    .fWrap = ptText->fWrap,
                    .uColor = ptText->uColor});
        }
    }

    gptDraw->submit_2d_layer(ptDrawlist->ptLayer);
    pl_submit_2d_drawlist(ptDrawlist->pt2dDrawlist, ptEncoder, fWidth, fHeight, uMSAASampleCount);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_draw_backend_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    const plDrawBackendI tApi = {
        .initialize         = pl_initialize_draw_backend,
        .cleanup            = pl_cleanup_draw_backend,
        .new_frame          = pl_new_draw_frame,
        .build_font_atlas   = pl_build_font_atlas_backend,
        .cleanup_font_atlas = pl_cleanup_font_atlas_backend,
        .submit_2d_drawlist = pl_submit_2d_drawlist,
        .submit_3d_drawlist = pl_submit_3d_drawlist,
        .create_bind_group_for_texture = pl_create_bind_group_for_texture,
    };
    pl_set_api(ptApiRegistry, plDrawBackendI, &tApi);

    if(bReload)
        gptDrawBackendCtx = gptDataRegistry->get_data("plDrawBackendContext");
    else  // first load
    {
        static plDrawBackendContext tCtx = {0};
        gptDrawBackendCtx = &tCtx;
        gptDataRegistry->set_data("plDrawBackendContext", gptDrawBackendCtx);
    }
}

static void
pl_unload_draw_backend_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plDrawBackendI* ptApi = pl_get_api(ptApiRegistry, plDrawBackendI);
    ptApiRegistry->remove(ptApi);
}