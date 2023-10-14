/*
   pl_2d_graphics_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_2d_graphics_ext.h"
#include "pl_ds.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ui.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#include "pl_graphics_ext.h"
#include "pl_image_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plTileMap
{
    plTile*       sbtTiles;
    uint32_t      uHorizontalTiles;
    uint32_t      uVerticalTiles;
    uint32_t      uTileSize;
    uint32_t      uMargin;
    plTexture     tTexture;
    plTextureView tTextureView;
    plBindGroup   tBindGroup1;
} plTileMap;

typedef struct _pl2DLayerDraw
{
    uint32_t uTileCount;
    uint32_t uIndexOffset;
    uint32_t uVertexOffset;
    uint32_t uDataOffset;
    plVec2   tOffset;
} pl2DLayerDraw;

typedef struct _pl2DLayer
{
    const char*    pcName;
    pl2DLayerDraw* sbtDraws;
    uint32_t       uTileMap;
} pl2DLayer;

typedef struct _BindGroup_0
{
    int iViewportWidth;  
    int iViewportHeight;
} BindGroup_0;

typedef struct _BindGroup_2
{
    plVec4 tShaderSpecific;
} BindGroup_2;

typedef struct _plDynamicData
{
    int    iDataOffset;
    int    iVertexOffset;
    plVec2 tOffset;
} plDynamicData;

typedef struct _pl2DContext
{

    plTileMap*  sbtTilemaps;
    pl2DLayer*  sbtLayersCreated;
    pl2DLayer** sbtLayersSubmitted;

    plDraw* sbtDraws;

    plShader tShader0;
    plBuffer atGlobalBuffers[2];

    // bind groups
    plBindGroup atBindGroups0[2];
    plBindGroup tBindGroup2;

    plBuffer  tShaderSpecificBuffer;
    plBuffer  tVertexBuffer;
    plBuffer  tIndexBuffer;
    plBuffer  tStorageBuffer;
    plVec4*   sbtVertexDataBuffer;
    plVec3*   sbtVertexPosBuffer;
    uint32_t* sbuIndexBuffer;

} pl2DContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// context 
static pl2DContext* gptCtx = NULL;

// apis
static const plGraphicsI*  gptGfx    = NULL;
static const plDeviceI*    gptDevice = NULL;
const plImageApiI*         gptImage  = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl_initialize_2d_graphics(plGraphics* ptGraphics)
{
    plShaderDescription tShaderDescription0 = {

#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/primitive_2d.metal",
        .pcPixelShader = "../shaders/metal/primitive_2d.metal",
#else // VULKAN
        .pcVertexShader = "primitive_2d.vert.spv",
        .pcPixelShader = "primitive_2d.frag.spv",
#endif

        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
            .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1
                    }
                },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                },
            }
        }
    };
    gptCtx->tShader0 = gptGfx->create_shader(ptGraphics, &tShaderDescription0);

    // shader specific buffer
    const BindGroup_2 tShaderSpecificBufferDesc = {
        .tShaderSpecific = {0.0f, 0.0f, 0.0f, 0.0f}
    };
    const plBufferDescription tShaderBufferDesc = {
        .pcDebugName          = "shader buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_2),
        .uInitialDataByteSize = sizeof(BindGroup_2),
        .puInitialData        = (uint8_t*)&tShaderSpecificBufferDesc
    };
    gptCtx->tShaderSpecificBuffer = gptDevice->create_buffer(&ptGraphics->tDevice, &tShaderBufferDesc);

    const plBufferDescription atGlobalBuffersDesc = {
        .pcDebugName          = "global buffer",
        .tMemory              = PL_MEMORY_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_0),
        .uInitialDataByteSize = 0,
        .puInitialData        = NULL
    };
    gptCtx->atGlobalBuffers[0] = gptDevice->create_buffer(&ptGraphics->tDevice, &atGlobalBuffersDesc);
    gptCtx->atGlobalBuffers[1] = gptDevice->create_buffer(&ptGraphics->tDevice, &atGlobalBuffersDesc);

    plBindGroupLayout tBindGroupLayout0 = {
        .uBufferCount  = 2,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1
            },
        }
    };
    gptCtx->atBindGroups0[0] = gptDevice->create_bind_group(&ptGraphics->tDevice, &tBindGroupLayout0);
    gptCtx->atBindGroups0[1] = gptDevice->create_bind_group(&ptGraphics->tDevice, &tBindGroupLayout0);

}

static void
pl_cleanup_2d_graphics(plGraphics* ptGraphics)
{
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtTilemaps); i++)
    {
        pl_sb_free(gptCtx->sbtTilemaps[i].sbtTiles);
    }

    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtLayersCreated); i++)
    {
        pl_sb_free(gptCtx->sbtLayersCreated[i].sbtDraws);
    }

    pl_sb_free(gptCtx->sbtTilemaps);
    pl_sb_free(gptCtx->sbtLayersCreated);
    pl_sb_free(gptCtx->sbtLayersSubmitted);
    pl_sb_free(gptCtx->sbtDraws);
    pl_sb_free(gptCtx->sbtVertexDataBuffer);
    pl_sb_free(gptCtx->sbtVertexPosBuffer);
    pl_sb_free(gptCtx->sbuIndexBuffer);
}

static uint32_t
pl_create_layer(const char* pcName, uint32_t uTileMap)
{
    const uint32_t uLayerHandle = pl_sb_size(gptCtx->sbtLayersCreated);

    const pl2DLayer tLayer = {
        .pcName   = pcName,
        .uTileMap = uTileMap
    };

    pl_sb_push(gptCtx->sbtLayersCreated, tLayer);
    return uLayerHandle;
}

static void
pl_compile_layers(plGraphics* ptGraphics)
{

    const plBufferDescription tStorageBufferDesc = {
        .pcDebugName          = "storage buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plVec4) * pl_sb_size(gptCtx->sbtVertexDataBuffer),
        .uInitialDataByteSize = sizeof(plVec4) * pl_sb_size(gptCtx->sbtVertexDataBuffer),
        .puInitialData        = (uint8_t*)gptCtx->sbtVertexDataBuffer
    };
    gptCtx->tStorageBuffer = gptDevice->create_buffer(&ptGraphics->tDevice, &tStorageBufferDesc);
    pl_sb_free(gptCtx->sbtVertexDataBuffer);

    size_t szBufferRangeSize[2] = {sizeof(BindGroup_0), tStorageBufferDesc.uByteSize};

    plBuffer atBindGroup0_buffers0[2] = {gptCtx->atGlobalBuffers[0], gptCtx->tStorageBuffer};
    plBuffer atBindGroup0_buffers1[2] = {gptCtx->atGlobalBuffers[1], gptCtx->tStorageBuffer};
    gptDevice->update_bind_group(&ptGraphics->tDevice, &gptCtx->atBindGroups0[0], 2, atBindGroup0_buffers0, szBufferRangeSize, 0, NULL);
    gptDevice->update_bind_group(&ptGraphics->tDevice, &gptCtx->atBindGroups0[1], 2, atBindGroup0_buffers1, szBufferRangeSize, 0, NULL);

    plBindGroupLayout tBindGroupLayout2 = {
        .uBufferCount  = 1,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0
            },
        }
    };
    gptCtx->tBindGroup2 = gptDevice->create_bind_group(&ptGraphics->tDevice, &tBindGroupLayout2);
    size_t szGroup2BufferRange = sizeof(BindGroup_2);
    gptDevice->update_bind_group(&ptGraphics->tDevice, &gptCtx->tBindGroup2, 1, &gptCtx->tShaderSpecificBuffer, &szGroup2BufferRange, 0, NULL);

    const plBufferDescription tIndexBufferDesc = {
        .pcDebugName          = "index buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_INDEX,
        .uByteSize            = sizeof(uint32_t) * pl_sb_size(gptCtx->sbuIndexBuffer),
        .uInitialDataByteSize = sizeof(uint32_t) * pl_sb_size(gptCtx->sbuIndexBuffer),
        .puInitialData        = (uint8_t*)gptCtx->sbuIndexBuffer
    };
    gptCtx->tIndexBuffer = gptDevice->create_buffer(&ptGraphics->tDevice, &tIndexBufferDesc);
    pl_sb_free(gptCtx->sbuIndexBuffer);

    const plBufferDescription tVertexBufferDesc = {
        .pcDebugName          = "vertex buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_VERTEX,
        .uByteSize            = sizeof(plVec3) * pl_sb_size(gptCtx->sbtVertexPosBuffer),
        .uInitialDataByteSize = sizeof(plVec3) * pl_sb_size(gptCtx->sbtVertexPosBuffer),
        .puInitialData        = (uint8_t*)gptCtx->sbtVertexPosBuffer
    };
    gptCtx->tVertexBuffer = gptDevice->create_buffer(&ptGraphics->tDevice, &tVertexBufferDesc);
    pl_sb_free(gptCtx->sbtVertexPosBuffer);  
}

static void
pl_submit_2d_layer(uint32_t uLayerHandle)
{
    pl2DLayer* ptLayer = &gptCtx->sbtLayersCreated[uLayerHandle];
    pl_sb_push(gptCtx->sbtLayersSubmitted, ptLayer);
}

static void
pl_draw_layers(plGraphics* ptGraphics)
{
    plDevice* ptDevice = &ptGraphics->tDevice;

    const BindGroup_0 tBindGroupBuffer = {
        .iViewportWidth  = (int)pl_get_io()->afMainViewportSize[0],
        .iViewportHeight = (int)pl_get_io()->afMainViewportSize[1]
    };
    memcpy(gptCtx->atGlobalBuffers[ptGraphics->uCurrentFrameIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    const uint32_t uLayerCount = pl_sb_size(gptCtx->sbtLayersSubmitted);
    for(uint32_t uLayerIndex = 0; uLayerIndex < uLayerCount; uLayerIndex++)
    {
        pl2DLayer* ptLayer = gptCtx->sbtLayersSubmitted[uLayerIndex];

        plTileMap* ptMap = &gptCtx->sbtTilemaps[ptLayer->uTileMap];
        
        for(uint32_t i = 0; i < pl_sb_size(ptLayer->sbtDraws); i++)
        {

            plDynamicBinding tDynamicBinding0 = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plDynamicData));
            plDynamicData* ptDynamicData0 = (plDynamicData*)tDynamicBinding0.pcData;
            ptDynamicData0->iDataOffset = (int)ptLayer->sbtDraws[i].uDataOffset;
            ptDynamicData0->iVertexOffset = (int)ptLayer->sbtDraws[i].uVertexOffset;
            ptDynamicData0->tOffset = ptLayer->sbtDraws[i].tOffset;

            plDraw tDraw = {
                    .uDynamicBuffer = tDynamicBinding0.uBufferHandle,
                    .uVertexBuffer = gptCtx->tVertexBuffer.uHandle,
                    .uIndexBuffer = gptCtx->tIndexBuffer.uHandle,
                    .uIndexCount = 6 * ptLayer->sbtDraws[i].uTileCount,
                    .uVertexCount = 4 * ptLayer->sbtDraws[i].uTileCount,
                    .uIndexOffset = ptLayer->sbtDraws[i].uIndexOffset,
                    .aptBindGroups = {
                        &gptCtx->atBindGroups0[ptGraphics->uCurrentFrameIndex],
                        &ptMap->tBindGroup1,
                        &gptCtx->tBindGroup2
                    },
                    .uShaderVariant = gptCtx->tShader0.uHandle,
                    .auDynamicBufferOffset = {
                        tDynamicBinding0.uByteOffset
                    }
            };

            pl_sb_push(gptCtx->sbtDraws, tDraw);
        }
    }

    plDrawArea tArea = {
        .uDrawOffset = 0,
        .uDrawCount = pl_sb_size(gptCtx->sbtDraws)
    };
    gptGfx->draw_areas(ptGraphics, 1, &tArea, gptCtx->sbtDraws);

    pl_sb_reset(gptCtx->sbtLayersSubmitted);
    pl_sb_reset(gptCtx->sbtDraws);
}

static uint32_t
pl_add_tile(uint32_t uLayerHandle, plTile tTile, plVec2 tPos, plVec2 tSize)
{
    pl2DLayer* ptLayer = &gptCtx->sbtLayersCreated[uLayerHandle];

    plTileMap* ptMap = &gptCtx->sbtTilemaps[tTile.uParentMap];

    const uint32_t uStartIndex = pl_sb_size(gptCtx->sbtVertexPosBuffer);

    const pl2DLayerDraw tDraw = {
        .uIndexOffset  = pl_sb_size(gptCtx->sbuIndexBuffer),
        .uDataOffset   = pl_sb_size(gptCtx->sbtVertexDataBuffer),
        .uVertexOffset = uStartIndex,
        .uTileCount    = 1
    };
    pl_sb_push(ptLayer->sbtDraws, tDraw);

    // indices
    pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 3);

    // vertices (position)
    pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){tPos.x, tPos.y, 0.0f}));
    pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){tPos.x, tPos.y + tSize.y, 0.0f}));
    pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){tPos.x + tSize.x, tPos.y + tSize.y, 0.0f}));
    pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){tPos.x + tSize.x, tPos.y, 0.0f}));

    // vertices (data - uv, color)
    pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tTopLeft.x, tTile.tTopLeft.y}));
    pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tBottomLeft.x, tTile.tBottomLeft.y}));
    pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tBottomRight.x, tTile.tBottomRight.y}));
    pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tTopRight.x, tTile.tTopRight.y}));  

    return pl_sb_size(ptLayer->sbtDraws) - 1;
}

static uint32_t
pl_add_tiles(uint32_t uLayerHandle, uint32_t uCount, plTile* ptTiles, plVec2* ptPositions, plVec2* ptSizes)
{
    pl2DLayer* ptLayer = &gptCtx->sbtLayersCreated[uLayerHandle];

    plTileMap* ptMap = &gptCtx->sbtTilemaps[ptLayer->uTileMap];

    const pl2DLayerDraw tDraw = {
        .uIndexOffset  = pl_sb_size(gptCtx->sbuIndexBuffer),
        .uDataOffset   = pl_sb_size(gptCtx->sbtVertexDataBuffer),
        .uVertexOffset = pl_sb_size(gptCtx->sbtVertexPosBuffer),
        .uTileCount    = uCount
    };
    pl_sb_push(ptLayer->sbtDraws, tDraw);


    for(uint32_t i = 0; i < uCount; i++)
    {

    const uint32_t uStartIndex = pl_sb_size(gptCtx->sbtVertexPosBuffer);

        // indices
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 3);

        // vertices (position)
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x, ptPositions[i].y, 0.0f}));
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x, ptPositions[i].y + ptSizes[i].y, 0.0f}));
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x + ptSizes[i].x, ptPositions[i].y + ptSizes[i].y, 0.0f}));
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x + ptSizes[i].x, ptPositions[i].y, 0.0f}));

        // vertices (data - uv, color)
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){ptTiles[i].tTopLeft.x, ptTiles[i].tTopLeft.y}));
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){ptTiles[i].tBottomLeft.x, ptTiles[i].tBottomLeft.y}));
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){ptTiles[i].tBottomRight.x, ptTiles[i].tBottomRight.y}));
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){ptTiles[i].tTopRight.x, ptTiles[i].tTopRight.y}));  

    }
    return pl_sb_size(ptLayer->sbtDraws) - 1;  
}

static uint32_t
pl_add_duplicated_tiles(uint32_t uLayerHandle, uint32_t uCount, plTile tTile, const plVec2* ptPositions, plVec2 tSize)
{
    pl2DLayer* ptLayer = &gptCtx->sbtLayersCreated[uLayerHandle];

    plTileMap* ptMap = &gptCtx->sbtTilemaps[ptLayer->uTileMap];

    const pl2DLayerDraw tDraw = {
        .uIndexOffset  = pl_sb_size(gptCtx->sbuIndexBuffer),
        .uDataOffset   = pl_sb_size(gptCtx->sbtVertexDataBuffer),
        .uVertexOffset = pl_sb_size(gptCtx->sbtVertexPosBuffer),
        .uTileCount    = uCount
    };
    pl_sb_push(ptLayer->sbtDraws, tDraw);


    for(uint32_t i = 0; i < uCount; i++)
    {

    const uint32_t uStartIndex = pl_sb_size(gptCtx->sbtVertexPosBuffer);

        // indices
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(gptCtx->sbuIndexBuffer, uStartIndex + 3);

        // vertices (position)
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x, ptPositions[i].y, 0.0f}));
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x, ptPositions[i].y + tSize.y, 0.0f}));
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x + tSize.x, ptPositions[i].y + tSize.y, 0.0f}));
        pl_sb_push(gptCtx->sbtVertexPosBuffer, ((plVec3){ptPositions[i].x + tSize.x, ptPositions[i].y, 0.0f}));

        // vertices (data - uv, color)
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tTopLeft.x, tTile.tTopLeft.y}));
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tBottomLeft.x, tTile.tBottomLeft.y}));
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tBottomRight.x, tTile.tBottomRight.y}));
        pl_sb_push(gptCtx->sbtVertexDataBuffer, ((plVec4){tTile.tTopRight.x, tTile.tTopRight.y}));  

    }
    return pl_sb_size(ptLayer->sbtDraws) - 1; 
}

static void
pl_update_tile(uint32_t uLayerHandle, uint32_t uTileHandle, plVec2 tPos)
{
    pl2DLayer* ptLayer = &gptCtx->sbtLayersCreated[uLayerHandle];
    ptLayer->sbtDraws[uTileHandle].tOffset = tPos;
}

static void
pl_get_tile(uint32_t uMap, uint32_t uX, uint32_t uY, plTile* ptTileOut)
{
    plTileMap* ptMap = &gptCtx->sbtTilemaps[uMap];
    PL_ASSERT(uX <= ptMap->uHorizontalTiles);
    PL_ASSERT(uY <= ptMap->uVerticalTiles);

    const uint32_t uTileIndex = uX + (uY * ptMap->uHorizontalTiles);

    *ptTileOut = ptMap->sbtTiles[uTileIndex];
}

static uint32_t
pl_create_tile_map(const char* pcPath, plGraphics* ptGraphics, uint32_t uHorizontalTiles, uint32_t uVerticalTiles, uint32_t uTileSize, uint32_t uTileMargin)
{
    plTileMap tMap = {0};
    plDevice* ptDevice = &ptGraphics->tDevice;
    tMap.uHorizontalTiles = uHorizontalTiles;
    tMap.uVerticalTiles = uVerticalTiles;
    tMap.uTileSize = uTileSize;
    tMap.uMargin = uTileMargin;

    pl_sb_resize(tMap.sbtTiles, tMap.uHorizontalTiles * tMap.uVerticalTiles);

    const float fImageWidth  = (float)(tMap.uHorizontalTiles * tMap.uTileSize + tMap.uMargin * (tMap.uHorizontalTiles - 1));
    const float fImageHeight = (float)(tMap.uVerticalTiles * tMap.uTileSize + tMap.uMargin * (tMap.uVerticalTiles - 1));

    const float fHorizontalMargin = (float)tMap.uMargin / fImageWidth;
    const float fVerticalMargin = (float)tMap.uMargin / fImageHeight;

    for(uint32_t i = 0; i < tMap.uHorizontalTiles; i++)
    {
        for(uint32_t j = 0; j < tMap.uVerticalTiles; j++)
        {
            const uint32_t uTileIndex = i + j * tMap.uHorizontalTiles;

            tMap.sbtTiles[uTileIndex].tTopLeft.x = (float)(i * (tMap.uTileSize + tMap.uMargin) ) / fImageWidth;
            tMap.sbtTiles[uTileIndex].tTopLeft.y = (float)(j * (tMap.uTileSize + tMap.uMargin) ) / fImageHeight;

            tMap.sbtTiles[uTileIndex].tBottomRight.x = -fHorizontalMargin + tMap.sbtTiles[uTileIndex].tTopLeft.x + (float)(tMap.uTileSize + tMap.uMargin) / fImageWidth;
            tMap.sbtTiles[uTileIndex].tBottomRight.y = -fVerticalMargin + tMap.sbtTiles[uTileIndex].tTopLeft.y + (float)(tMap.uTileSize + tMap.uMargin) / fImageHeight;

            tMap.sbtTiles[uTileIndex].tTopRight.x = tMap.sbtTiles[uTileIndex].tBottomRight.x;
            tMap.sbtTiles[uTileIndex].tTopRight.y = tMap.sbtTiles[uTileIndex].tTopLeft.y;

            tMap.sbtTiles[uTileIndex].tBottomLeft.x = tMap.sbtTiles[uTileIndex].tTopLeft.x;
            tMap.sbtTiles[uTileIndex].tBottomLeft.y = tMap.sbtTiles[uTileIndex].tBottomRight.y;

            tMap.sbtTiles[uTileIndex].uParentMap = pl_sb_size(gptCtx->sbtTilemaps);
        }
    }

    int iUnused;
    int iTileMapWidth;
    int iTileMapHeight;
    unsigned char* pucImageData = gptImage->load(pcPath, &iTileMapWidth, &iTileMapHeight, &iUnused, 4);

    plTextureDesc tTextureDesc = {
        .tDimensions = {(float)iTileMapWidth, (float)iTileMapHeight, 1},
        .tFormat = PL_FORMAT_R8G8B8A8_UNORM,
        .uLayers = 1,
        .uMips = 0
    };
    tMap.tTexture = gptDevice->create_texture(ptDevice, tTextureDesc, iTileMapWidth * iTileMapHeight * 4, pucImageData, pcPath);

    gptImage->free(pucImageData);

    plTextureViewDesc tTextureViewDesc = {
        .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    plSampler tSampler = {
        .tFilter = PL_FILTER_NEAREST,
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tVerticalWrap = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };
    tMap.tTextureView = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, &tMap.tTexture, pcPath);

    plBindGroupLayout tBindGroupLayout1 = {
        .uTextureCount  = 1,
        .aTextures = {
            {.uSlot = 0}
        }
    };
    tMap.tBindGroup1 = gptDevice->create_bind_group(ptDevice, &tBindGroupLayout1);
    gptDevice->update_bind_group(ptDevice, &tMap.tBindGroup1, 0, NULL, NULL, 1, &tMap.tTextureView);

    pl_sb_push(gptCtx->sbtTilemaps, tMap);
    return pl_sb_size(gptCtx->sbtTilemaps) - 1;
}

static void*
pl_get_ui_texture(plGraphics* ptGraphics, uint32_t uTileMapHandle)
{
    return gptGfx->get_ui_texture_handle(ptGraphics, &gptCtx->sbtTilemaps[uTileMapHandle].tTextureView);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

const pl2DGraphicsI*
pl_load_2d_graphics_api(void)
{
    static const pl2DGraphicsI tApi = {
        .initialize           = pl_initialize_2d_graphics,
        .cleanup              = pl_cleanup_2d_graphics,
        .create_tile_map      = pl_create_tile_map,
        .get_tile             = pl_get_tile,
        .create_layer         = pl_create_layer,
        .submit_layer         = pl_submit_2d_layer,
        .compile_layers       = pl_compile_layers,
        .add_tile             = pl_add_tile,
        .add_tiles            = pl_add_tiles,
        .add_duplicated_tiles = pl_add_duplicated_tiles,
        .draw_layers          = pl_draw_layers,
        .update_tile          = pl_update_tile,
        .get_ui_texture       = pl_get_ui_texture
    };
    return &tApi;
}

PL_EXPORT void
pl_load_2d_graphics_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_log_context(ptDataRegistry->get_data("log"));
    pl_set_profile_context(ptDataRegistry->get_data("profile"));
    pl_set_context(ptDataRegistry->get_data("ui"));
    gptGfx = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice = ptApiRegistry->first(PL_API_DEVICE);
    gptImage = ptApiRegistry->first(PL_API_IMAGE);

    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_2D_GRAPHICS), pl_load_2d_graphics_api());
    else
    {
        static pl2DContext tContext = {0};
        gptCtx = &tContext;
        ptApiRegistry->add(PL_API_2D_GRAPHICS, pl_load_2d_graphics_api());
    }
}

PL_EXPORT void
pl_unload_2d_graphics_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}
