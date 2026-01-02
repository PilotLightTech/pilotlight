/*
   pl_draw_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal structs
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

#include <float.h>
#include <stdlib.h>
#include "pl.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_draw_ext.h"
#include "pl_memory.h"
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#endif

// stb libs
#include "stb_rect_pack.h"
#include "stb_truetype.h"

// extensions
#include "pl_vfs_ext.h" // file

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_DRAWLISTS
    #define PL_MAX_DRAWLISTS 64
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plFontCustomRect
{
    uint32_t       uWidth;
    uint32_t       uHeight;
    uint32_t       uX;
    uint32_t       uY;
    unsigned char* pucBytes;
} plFontCustomRect;

typedef struct _plFontChar
{
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
    float    xOff;
    float    yOff;
    float    xAdv;
    float    xOff2;
    float    yOff2;
} plFontChar;

typedef struct _plDrawLayer2D
{
    plDrawList2D*  ptDrawlist;
    plDrawCommand* sbtCommandBuffer;
    uint32_t*      sbuIndexBuffer;
    plVec2*        sbtPath;
    uint32_t       uVertexCount;
    plDrawCommand* ptLastCommand;
} plDrawLayer2D;

typedef struct _plFontPrepData
{
    stbtt_fontinfo    tFontInfo;
    stbtt_pack_range* ptRanges;
    stbrp_rect*       ptRects;
    uint32_t          uTotalCharCount;
    float             fScale;
    bool              bPrepped;
    float             fAscent;
    float             fDescent;
} plFontPrepData;

typedef struct _plDrawContext
{
    // 2D resources
    plPoolAllocator tDrawlistPool2D;
    plDrawList2D    atDrawlists2DBuffer[PL_MAX_DRAWLISTS];
    plDrawList2D*   aptDrawlists2D[PL_MAX_DRAWLISTS];
    uint32_t        uDrawlistCount2D;

    // 3D resources
    plPoolAllocator tDrawlistPool3D;
    plDrawList3D    atDrawlists3DBuffer[PL_MAX_DRAWLISTS];
    plDrawList3D*   aptDrawlists3D[PL_MAX_DRAWLISTS];
    uint32_t        uDrawlistCount3D;

    // current font
    plFontAtlas* ptAtlas;

    plTempAllocator tTempAllocator;
} plDrawContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plDrawContext* gptDrawCtx = NULL;

static unsigned char*        ptrBarrierOutE_ = NULL;
static unsigned char*        ptrBarrierOutB_ = NULL;
static const unsigned char * ptrBarrierInB_;
static unsigned char*        ptrDOut_ = NULL;

#ifndef PL_UNITY_BUILD
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plVfsI* gptVfs = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void pl__prepare_draw_command(plDrawLayer2D*, plTextureID, bool sdf);
static void pl__reserve_triangles   (plDrawLayer2D*, uint32_t uIndexCount, uint32_t uVertexCount);
static void pl__add_vertex          (plDrawLayer2D*, plVec2 tPos, uint32_t uColor, plVec2 tUv);
static void pl__add_index           (plDrawLayer2D*, uint32_t uVertexStart, uint32_t i0, uint32_t i1, uint32_t i2);

static const plFontGlyph* pl__find_glyph(plFont* ptFont, uint32_t c);

// math
#define pl__add_vec2(left, right)      (plVec2){(left).x + (right).x, (left).y + (right).y}
#define pl__subtract_vec2(left, right) (plVec2){(left).x - (right).x, (left).y - (right).y}
#define pl__mul_vec2_f(left, right)    (plVec2){(left).x * (right), (left).y * (right)}
#define pl__mul_f_vec2(left, right)    (plVec2){(left) * (right).x, (left) * (right).y}

// stateful drawing
#define pl__submit_path(ptLayer, tOptions)\
    pl_add_lines((ptLayer), (ptLayer)->sbtPath, pl_sb_size((ptLayer)->sbtPath), (tOptions));\
    pl_sb_reset((ptLayer)->sbtPath);

#define PL_NORMALIZE2F_OVER_ZERO(VX,VY) \
    { float d2 = (VX) * (VX) + (VY) * (VY); \
    if (d2 > 0.0f) { float inv_len = 1.0f / sqrtf(d2); (VX) *= inv_len; (VY) *= inv_len; } } (void)0

static inline void
pl__add_3d_indexed_lines(
    plDrawList3D* ptDrawlist, uint32_t uIndexCount, const plVec3* atPoints,
    const uint32_t* auIndices, plDrawLineOptions tOptions)
{

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtLineVertexBuffer);
    const uint32_t uIndexStart = pl_sb_size(ptDrawlist->sbtLineIndexBuffer);
    const uint32_t uLineCount = uIndexCount / 2;

    pl_sb_resize(ptDrawlist->sbtLineVertexBuffer, uVertexStart + 4 * uLineCount);
    pl_sb_resize(ptDrawlist->sbtLineIndexBuffer, uIndexStart + 6 * uLineCount);

    uint32_t uCurrentVertex = uVertexStart;
    uint32_t uCurrentIndex = uIndexStart;
    for(uint32_t i = 0; i < uLineCount; i++)
    {
        const uint32_t uIndex0 = auIndices[i * 2];
        const uint32_t uIndex1 = auIndices[i * 2 + 1];

        const plVec3 tP0 = atPoints[uIndex0];
        const plVec3 tP1 = atPoints[uIndex1];

        plDrawVertex3DLine tNewVertex0 = {
            {tP0.x, tP0.y, tP0.z},
            -1.0f,
            tOptions.fThickness,
            1.0f,
            {tP1.x, tP1.y, tP1.z},
            tOptions.uColor
        };

        plDrawVertex3DLine tNewVertex1 = {
            {tP1.x, tP1.y, tP1.z},
            -1.0f,
            tOptions.fThickness,
            -1.0f,
            {tP0.x, tP0.y, tP0.z},
            tOptions.uColor
        };

        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex] = tNewVertex0;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 1] = tNewVertex1;

        tNewVertex0.fDirection = 1.0f;
        tNewVertex1.fDirection = 1.0f;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 2] = tNewVertex1;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 3] = tNewVertex0;

        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex] = uCurrentVertex + 0;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 1] = uCurrentVertex + 1;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 2] = uCurrentVertex + 2;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 3] = uCurrentVertex + 0;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 4] = uCurrentVertex + 2;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 5] = uCurrentVertex + 3;

        uCurrentVertex += 4;
        uCurrentIndex += 6;
    }
}

static inline void
pl__add_3d_lines(plDrawList3D* ptDrawlist, uint32_t uCount, const plVec3* atPoints, plDrawLineOptions tOptions)
{
    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtLineVertexBuffer);
    const uint32_t uIndexStart = pl_sb_size(ptDrawlist->sbtLineIndexBuffer);

    pl_sb_resize(ptDrawlist->sbtLineVertexBuffer, uVertexStart + 4 * uCount);
    pl_sb_resize(ptDrawlist->sbtLineIndexBuffer, uIndexStart + 6 * uCount);

    uint32_t uCurrentVertex = uVertexStart;
    uint32_t uCurrentIndex = uIndexStart;
    for(uint32_t i = 0; i < uCount; i++)
    {
        const plVec3 tP0 = atPoints[i * 2];
        const plVec3 tP1 = atPoints[i * 2 + 1];

        plDrawVertex3DLine tNewVertex0 = {
            {tP0.x, tP0.y, tP0.z},
            -1.0f,
            tOptions.fThickness,
            1.0f,
            {tP1.x, tP1.y, tP1.z},
            tOptions.uColor
        };

        plDrawVertex3DLine tNewVertex1 = {
            {tP1.x, tP1.y, tP1.z},
            -1.0f,
            tOptions.fThickness,
            -1.0f,
            {tP0.x, tP0.y, tP0.z},
            tOptions.uColor
        };

        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex] = tNewVertex0;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 1] = tNewVertex1;

        tNewVertex0.fDirection = 1.0f;
        tNewVertex1.fDirection = 1.0f;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 2] = tNewVertex1;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 3] = tNewVertex0;

        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex] = uCurrentVertex + 0;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 1] = uCurrentVertex + 1;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 2] = uCurrentVertex + 2;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 3] = uCurrentVertex + 0;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 4] = uCurrentVertex + 2;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 5] = uCurrentVertex + 3;

        uCurrentVertex += 4;
        uCurrentIndex += 6;
    }
}

static inline void
pl__add_3d_path(plDrawList3D* ptDrawlist, uint32_t uCount, const plVec3* atPoints, plDrawLineOptions tOptions)
{
    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtLineVertexBuffer);
    const uint32_t uIndexStart = pl_sb_size(ptDrawlist->sbtLineIndexBuffer);

    pl_sb_resize(ptDrawlist->sbtLineVertexBuffer, uVertexStart + 4 * (uCount - 1));
    pl_sb_resize(ptDrawlist->sbtLineIndexBuffer, uIndexStart + 6 * (uCount - 1));

    uint32_t uCurrentVertex = uVertexStart;
    uint32_t uCurrentIndex = uIndexStart;
    for(uint32_t i = 0; i < uCount - 1; i++)
    {
        const plVec3 tP0 = atPoints[i];
        const plVec3 tP1 = atPoints[i + 1];

        plDrawVertex3DLine tNewVertex0 = {
            {tP0.x, tP0.y, tP0.z},
            -1.0f,
            tOptions.fThickness,
            1.0f,
            {tP1.x, tP1.y, tP1.z},
            tOptions.uColor
        };

        plDrawVertex3DLine tNewVertex1 = {
            {tP1.x, tP1.y, tP1.z},
            -1.0f,
            tOptions.fThickness,
            -1.0f,
            {tP0.x, tP0.y, tP0.z},
            tOptions.uColor
        };

        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex] = tNewVertex0;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 1] = tNewVertex1;

        tNewVertex0.fDirection = 1.0f;
        tNewVertex1.fDirection = 1.0f;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 2] = tNewVertex1;
        ptDrawlist->sbtLineVertexBuffer[uCurrentVertex + 3] = tNewVertex0;

        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex] = uCurrentVertex + 0;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 1] = uCurrentVertex + 1;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 2] = uCurrentVertex + 2;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 3] = uCurrentVertex + 0;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 4] = uCurrentVertex + 2;
        ptDrawlist->sbtLineIndexBuffer[uCurrentIndex + 5] = uCurrentVertex + 3;

        uCurrentVertex += 4;
        uCurrentIndex += 6;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl_initialize(const plDrawInit* ptInit)
{
    size_t szBufferSize = sizeof(gptDrawCtx->atDrawlists3DBuffer);
    size_t szItems = pl_pool_allocator_init(&gptDrawCtx->tDrawlistPool3D, 0, sizeof(plDrawList3D), 0,
        &szBufferSize, gptDrawCtx->atDrawlists3DBuffer);
    pl_pool_allocator_init(&gptDrawCtx->tDrawlistPool3D, szItems, sizeof(plDrawList3D), 0,
        &szBufferSize, gptDrawCtx->atDrawlists3DBuffer);

    szBufferSize = sizeof(gptDrawCtx->atDrawlists2DBuffer);
    szItems = pl_pool_allocator_init(&gptDrawCtx->tDrawlistPool2D, 0, sizeof(plDrawList2D), 0,
        &szBufferSize, gptDrawCtx->atDrawlists2DBuffer);
    pl_pool_allocator_init(&gptDrawCtx->tDrawlistPool2D, szItems, sizeof(plDrawList2D), 0,
        &szBufferSize, gptDrawCtx->atDrawlists2DBuffer);
}

static void
pl_cleanup(void)
{

    for(uint32_t i = 0; i < gptDrawCtx->uDrawlistCount3D; i++)
    {
        plDrawList3D* ptDrawlist = gptDrawCtx->aptDrawlists3D[i];
        pl_sb_free(ptDrawlist->sbtSolidIndexBuffer);
        pl_sb_free(ptDrawlist->sbtSolidVertexBuffer);
        pl_sb_free(ptDrawlist->sbtLineVertexBuffer);
        pl_sb_free(ptDrawlist->sbtLineIndexBuffer);
        pl_sb_free(ptDrawlist->sbtTextEntries);
    }
    for(uint32_t i = 0; i < gptDrawCtx->uDrawlistCount2D; i++)
    {
        plDrawList2D* ptDrawlist = gptDrawCtx->aptDrawlists2D[i];
        pl_sb_free(ptDrawlist->_sbtClipStack);
        pl_sb_free(ptDrawlist->sbtDrawCommands);
        pl_sb_free(ptDrawlist->_sbtLayerCache);
        pl_sb_free(ptDrawlist->_sbtSubmittedLayers);
        pl_sb_free(ptDrawlist->sbtVertexBuffer);
        pl_sb_free(ptDrawlist->sbuIndexBuffer);

        for(uint32_t j = 0; j < pl_sb_size(ptDrawlist->_sbtLayersCreated); j++)
        {
            pl_sb_free(ptDrawlist->_sbtLayersCreated[j]->sbtCommandBuffer);
            pl_sb_free(ptDrawlist->_sbtLayersCreated[j]->sbtPath);
            pl_sb_free(ptDrawlist->_sbtLayersCreated[j]->sbuIndexBuffer);
            PL_FREE(ptDrawlist->_sbtLayersCreated[j]);
        }
        pl_sb_free(ptDrawlist->_sbtLayersCreated);
    }
    pl_temp_allocator_free(&gptDrawCtx->tTempAllocator);
}

static plDrawList2D*
pl_request_2d_drawlist(void)
{
    plDrawList2D* ptDrawlist = pl_pool_allocator_alloc(&gptDrawCtx->tDrawlistPool2D);

    if(ptDrawlist)
    {
        pl_sb_reserve(ptDrawlist->sbtVertexBuffer, 1024);
        gptDrawCtx->aptDrawlists2D[gptDrawCtx->uDrawlistCount2D] = ptDrawlist;
        gptDrawCtx->uDrawlistCount2D++;
    }
    return ptDrawlist;
}

static plDrawLayer2D*
pl_request_2d_layer(plDrawList2D* ptDrawlist)
{
   plDrawLayer2D* ptLayer = NULL;
   
   // check if ptDrawlist has any cached layers
   // which reduces allocations necessary since
   // cached layers' buffers are only reset
   if(pl_sb_size(ptDrawlist->_sbtLayerCache) > 0)
   {
        ptLayer = pl_sb_pop(ptDrawlist->_sbtLayerCache);
   }

   else // create new layer
   {
        ptLayer = PL_ALLOC(sizeof(plDrawLayer2D));
        memset(ptLayer, 0, sizeof(plDrawLayer2D));
        ptLayer->ptDrawlist = ptDrawlist;
        pl_sb_push(ptDrawlist->_sbtLayersCreated, ptLayer);
   }
   pl_sb_reserve(ptLayer->sbuIndexBuffer, 1024);
   return ptLayer;
}

static plDrawList3D*
pl_request_3d_drawlist(void)
{
    plDrawList3D* ptDrawlist = pl_pool_allocator_alloc(&gptDrawCtx->tDrawlistPool3D);

    if(ptDrawlist)
    {
        pl_sb_reserve(ptDrawlist->sbtLineIndexBuffer, 1024);
        pl_sb_reserve(ptDrawlist->sbtLineVertexBuffer, 1024);
        pl_sb_reserve(ptDrawlist->sbtSolidIndexBuffer, 1024);
        pl_sb_reserve(ptDrawlist->sbtSolidVertexBuffer, 1024);

        gptDrawCtx->aptDrawlists3D[gptDrawCtx->uDrawlistCount3D] = ptDrawlist;
        gptDrawCtx->uDrawlistCount3D++;

        if(ptDrawlist->pt2dDrawlist == NULL)
        {
            ptDrawlist->pt2dDrawlist = pl_request_2d_drawlist();
            ptDrawlist->ptLayer = pl_request_2d_layer(ptDrawlist->pt2dDrawlist);
        }
    }
    return ptDrawlist;
}

static void
pl_return_2d_drawlist(plDrawList2D* ptDrawlist)
{
    pl_sb_free(ptDrawlist->sbtVertexBuffer);
    pl_sb_free(ptDrawlist->_sbtLayerCache);
    pl_sb_free(ptDrawlist->_sbtSubmittedLayers)

    for(uint32_t j = 0; j < pl_sb_size(ptDrawlist->_sbtLayersCreated); j++)
    {
        pl_sb_free(ptDrawlist->_sbtLayersCreated[j]->sbtCommandBuffer);
        pl_sb_free(ptDrawlist->_sbtLayersCreated[j]->sbtPath);
        pl_sb_free(ptDrawlist->_sbtLayersCreated[j]->sbuIndexBuffer);
        PL_FREE(ptDrawlist->_sbtLayersCreated[j]);
    }
    pl_sb_free(ptDrawlist->_sbtLayersCreated);

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < gptDrawCtx->uDrawlistCount2D; i++)
    {
        if(gptDrawCtx->aptDrawlists2D[i] != ptDrawlist) // skip returning drawlist
        {
            plDrawList2D* ptCurrentDrawlist = gptDrawCtx->aptDrawlists2D[i];
            gptDrawCtx->aptDrawlists2D[uCurrentIndex] = ptCurrentDrawlist;
            uCurrentIndex++;
        }
    }
    pl_pool_allocator_free(&gptDrawCtx->tDrawlistPool2D, ptDrawlist);
    gptDrawCtx->uDrawlistCount2D--;
}

static void
pl_return_2d_layer(plDrawLayer2D* ptLayer)
{
    ptLayer->ptLastCommand = NULL;
    ptLayer->uVertexCount = 0;
    pl_sb_reset(ptLayer->sbtCommandBuffer);
    pl_sb_reset(ptLayer->sbuIndexBuffer);
    pl_sb_reset(ptLayer->sbtPath);
    pl_sb_push(ptLayer->ptDrawlist->_sbtLayerCache, ptLayer);
}

static void
pl_return_3d_drawlist(plDrawList3D* ptDrawlist)
{
    pl_return_2d_layer(ptDrawlist->ptLayer);
    pl_return_2d_drawlist(ptDrawlist->pt2dDrawlist);
    pl_sb_free(ptDrawlist->sbtLineIndexBuffer);
    pl_sb_free(ptDrawlist->sbtLineVertexBuffer);
    pl_sb_free(ptDrawlist->sbtSolidIndexBuffer);
    pl_sb_free(ptDrawlist->sbtSolidVertexBuffer);

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < gptDrawCtx->uDrawlistCount3D; i++)
    {
        if(gptDrawCtx->aptDrawlists3D[i] != ptDrawlist) // skip returning drawlist
        {
            plDrawList3D* ptCurrentDrawlist = gptDrawCtx->aptDrawlists3D[i];
            gptDrawCtx->aptDrawlists3D[uCurrentIndex] = ptCurrentDrawlist;
            uCurrentIndex++;
        }
    }
    pl_pool_allocator_free(&gptDrawCtx->tDrawlistPool3D, ptDrawlist);
    gptDrawCtx->uDrawlistCount3D--;
}

static void
pl_prepare_2d_drawlist(plDrawList2D* ptDrawlist)
{
    uint32_t uGlobalIdxBufferIndexOffset = 0u;
    const uint32_t uLayerCount = pl_sb_size(ptDrawlist->_sbtSubmittedLayers);
    for(uint32_t i = 0; i < uLayerCount; i++)
    {
        plDrawLayer2D* ptLayer = ptDrawlist->_sbtSubmittedLayers[i];
        plDrawCommand* ptLastCommand = NULL;

        // attempt to merge commands
        const uint32_t uCmdCount = pl_sb_size(ptLayer->sbtCommandBuffer);
        for(uint32_t j = 0; j < uCmdCount; j++)
        {
            plDrawCommand* ptLayerCommand = &ptLayer->sbtCommandBuffer[j];
            bool bCreateNewCommand = true;

            if(ptLastCommand)
            {
                // check for same texture (allows merging draw calls)
                if(ptLastCommand->tTextureId == ptLayerCommand->tTextureId && ptLastCommand->bSdf == ptLayerCommand->bSdf)
                {
                    // ptLastCommand->uElementCount += ptLayerCommand->uElementCount;
                    bCreateNewCommand = false;
                }

                // check for same clipping (allows merging draw calls)
                if(ptLayerCommand->tClip.tMax.x != ptLastCommand->tClip.tMax.x ||
                    ptLayerCommand->tClip.tMax.y != ptLastCommand->tClip.tMax.y ||
                    ptLayerCommand->tClip.tMin.x != ptLastCommand->tClip.tMin.x ||
                    ptLayerCommand->tClip.tMin.y != ptLastCommand->tClip.tMin.y)
                {
                    bCreateNewCommand = true;
                }

                // check for same callback (allows merging draw calls)5
                if(ptLastCommand->tUserCallback != NULL)
                {
                    bCreateNewCommand = true;
                }

                if(!bCreateNewCommand)
                {
                    ptLastCommand->uElementCount += ptLayerCommand->uElementCount;
                }
                
            }

            if(bCreateNewCommand)
            {
                ptLayerCommand->uIndexOffset = uGlobalIdxBufferIndexOffset + ptLayerCommand->uIndexOffset;
                pl_sb_push(ptDrawlist->sbtDrawCommands, *ptLayerCommand);       
                ptLastCommand = ptLayerCommand;
            }
            
        }    
        uGlobalIdxBufferIndexOffset += pl_sb_size(ptLayer->sbuIndexBuffer);    
    }
}

static void
pl_submit_2d_layer(plDrawLayer2D* ptLayer)
{
    pl_sb_push(ptLayer->ptDrawlist->_sbtSubmittedLayers, ptLayer);
    const uint32_t uCurrentIndexCount = pl_sb_size(ptLayer->ptDrawlist->sbuIndexBuffer);
    const uint32_t uAdditionalIndexCount = pl_sb_size(ptLayer->sbuIndexBuffer);
    if(uAdditionalIndexCount == 0)
        return;
    ptLayer->ptDrawlist->uIndexBufferByteSize += uAdditionalIndexCount * sizeof(uint32_t);
    pl_sb_add_n(ptLayer->ptDrawlist->sbuIndexBuffer, uAdditionalIndexCount);
    memcpy(&ptLayer->ptDrawlist->sbuIndexBuffer[uCurrentIndexCount], ptLayer->sbuIndexBuffer,
        uAdditionalIndexCount * sizeof(uint32_t));
}

static void
pl_add_lines(plDrawLayer2D* ptLayer, plVec2* atPoints, uint32_t uCount, plDrawLineOptions tOptions)
{
    uint32_t uSegmentCount = uCount - 1;
    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 6 * uSegmentCount, 4 * uSegmentCount);

    const float fThickness = tOptions.fThickness / 2.0f;

    for(uint32_t i = 0; i < uSegmentCount; i++)
    {
        float dx = atPoints[i + 1].x - atPoints[i].x;
        float dy = atPoints[i + 1].y - atPoints[i].y;
        PL_NORMALIZE2F_OVER_ZERO(dx, dy);

        const plVec2 tNormalVector = {
            .x = dy * fThickness,
            .y = -dx * fThickness
        };

        const plVec2 atCornerPoints[4] = 
        {
            pl__subtract_vec2(atPoints[i],     tNormalVector),
            pl__subtract_vec2(atPoints[i + 1], tNormalVector),
            pl__add_vec2(     atPoints[i + 1], tNormalVector),
            pl__add_vec2(     atPoints[i],     tNormalVector)
        };

        const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
        pl__add_vertex(ptLayer, atCornerPoints[0], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
        pl__add_vertex(ptLayer, atCornerPoints[1], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
        pl__add_vertex(ptLayer, atCornerPoints[2], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
        pl__add_vertex(ptLayer, atCornerPoints[3], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);

        pl__add_index(ptLayer, uVertexStart, 0, 1, 2);
        pl__add_index(ptLayer, uVertexStart, 0, 2, 3);
    }  
}

void
pl_add_2d_callback(plDrawLayer2D* ptLayer, plDrawCallback tCallback, void* pUserData, uint32_t uUserDataSize)
{

    plDrawCommand tNewDrawCommand =
    {
        .tUserCallback         = tCallback,
        .uUserCallbackDataSize = uUserDataSize,
        .pUserCallbackData     = pUserData
    };
    pl_sb_push(ptLayer->sbtCommandBuffer, tNewDrawCommand);

    ptLayer->ptLastCommand = NULL;
}

void
pl_set_2d_shader(plDrawLayer2D* ptLayer, plShaderHandle* ptShader)
{
    // create a shader-only command (no draw data, just shader switch)
    // backend will bind this shader when processing the command stream
    plDrawCommand tNewDrawCommand =
    {
        .ptUserShader = ptShader  // NULL clears user shader, non-NULL activates it
    };
    pl_sb_push(ptLayer->sbtCommandBuffer, tNewDrawCommand);

    // force next draw primitive to create a new command rather than
    // merging with the previous one - ensures draws after shader switch
    // get their own commands that will use the new shader
    ptLayer->ptLastCommand = NULL;
}

static void
pl_add_line(plDrawLayer2D* ptLayer, plVec2 p0, plVec2 p1, plDrawLineOptions tOptions)
{
    pl_sb_push(ptLayer->sbtPath, p0);
    pl_sb_push(ptLayer->sbtPath, p1);
    pl__submit_path(ptLayer, tOptions);
}

static void
pl_add_text_ex(plDrawLayer2D* ptLayer, plVec2 p, const char* pcText, plDrawTextOptions tOptions)
{

    if(tOptions.pcTextEnd == NULL)
    {
        tOptions.pcTextEnd = pcText;
        while(*tOptions.pcTextEnd != '\0')
            tOptions.pcTextEnd++;
    }

    plFont* ptFont = tOptions.ptFont;
    const float fSize = tOptions.fSize == 0.0f ? ptFont->fSize : tOptions.fSize;
    const char* pcTextEnd = tOptions.pcTextEnd;

    const float fScale = fSize > 0.0f ? fSize / ptFont->fSize : 1.0f;

    float fLineSpacing = fScale * ptFont->_fLineSpacing;
    const plVec2 tOriginalPosition = p;
    bool bFirstCharacter = true;
    bool bNoTransform = tOptions.tTransform.x11 == 0;

    while(pcText < pcTextEnd)
    {
        uint32_t c = (uint32_t)*pcText;
        if(c < 0x80)
            pcText += 1;
        else
        {
            pcText += pl_text_char_from_utf8(&c, pcText, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            p.x = tOriginalPosition.x;
            p.y += fLineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else if(bNoTransform)
        {

            const plFontGlyph* ptGlyph = pl__find_glyph(ptFont, c);
      
            float x0,y0,s0,t0; // top-left
            float x1,y1,s1,t1; // bottom-right

            // adjust for left side bearing if first char
            if(bFirstCharacter)
            {
                if(ptGlyph->fLeftBearing > 0.0f) p.x += ptGlyph->fLeftBearing * fScale;
                bFirstCharacter = false;
            }

            x0 = p.x + ptGlyph->x0 * fScale;
            x1 = p.x + ptGlyph->x1 * fScale;
            y0 = p.y + ptGlyph->y0 * fScale;
            y1 = p.y + ptGlyph->y1 * fScale;

            if(tOptions.fWrap > 0.0f && x1 > tOriginalPosition.x + tOptions.fWrap)
            {
                x0 = tOriginalPosition.x + ptGlyph->x0 * fScale;
                y0 = y0 + fLineSpacing;
                x1 = tOriginalPosition.x + ptGlyph->x1 * fScale;
                y1 = y1 + fLineSpacing;

                p.x = tOriginalPosition.x;
                p.y += fLineSpacing;
            }
            s0 = ptGlyph->u0;
            t0 = ptGlyph->v0;
            s1 = ptGlyph->u1;
            t1 = ptGlyph->v1;

            p.x += ptGlyph->fXAdvance * fScale;
            if(c != ' ')
            {
                pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, (bool)ptGlyph->iSDF);
                pl__reserve_triangles(ptLayer, 6, 4);
                const uint32_t uVtxStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
                pl__add_vertex(ptLayer, (plVec2){x0, y0}, tOptions.uColor, (plVec2){s0, t0});
                pl__add_vertex(ptLayer, (plVec2){x1, y0}, tOptions.uColor, (plVec2){s1, t0});
                pl__add_vertex(ptLayer, (plVec2){x1, y1}, tOptions.uColor, (plVec2){s1, t1});
                pl__add_vertex(ptLayer, (plVec2){x0, y1}, tOptions.uColor, (plVec2){s0, t1});

                pl__add_index(ptLayer, uVtxStart, 1, 0, 2);
                pl__add_index(ptLayer, uVtxStart, 2, 0, 3);
            }
        }
        else
        {

            const plFontGlyph* ptGlyph = pl__find_glyph(ptFont, c);
      
            float x0,y0,s0,t0; // top-left
            float x1,y1,s1,t1; // bottom-right

            // adjust for left side bearing if first char
            if(bFirstCharacter)
            {
                if(ptGlyph->fLeftBearing > 0.0f) p.x += ptGlyph->fLeftBearing * fScale;
                bFirstCharacter = false;
            }

            x0 = p.x + ptGlyph->x0 * fScale;
            x1 = p.x + ptGlyph->x1 * fScale;
            y0 = p.y + ptGlyph->y0 * fScale;
            y1 = p.y + ptGlyph->y1 * fScale;

            if(tOptions.fWrap > 0.0f && x1 > tOriginalPosition.x + tOptions.fWrap)
            {
                x0 = tOriginalPosition.x + ptGlyph->x0 * fScale;
                y0 = y0 + fLineSpacing;
                x1 = tOriginalPosition.x + ptGlyph->x1 * fScale;
                y1 = y1 + fLineSpacing;

                p.x = tOriginalPosition.x;
                p.y += fLineSpacing;
            }
            s0 = ptGlyph->u0;
            t0 = ptGlyph->v0;
            s1 = ptGlyph->u1;
            t1 = ptGlyph->v1;

            p.x += ptGlyph->fXAdvance * fScale;
            if(c != ' ')
            {
                pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, (bool)ptGlyph->iSDF);
                pl__reserve_triangles(ptLayer, 6, 4);
                const uint32_t uVtxStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
                pl__add_vertex(ptLayer, pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x0 - tOriginalPosition.x, y0 - tOriginalPosition.y, 1.0f}).xy), tOptions.uColor, (plVec2){s0, t0});
                pl__add_vertex(ptLayer, pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x1 - tOriginalPosition.x, y0 - tOriginalPosition.y, 1.0f}).xy), tOptions.uColor, (plVec2){s1, t0});
                pl__add_vertex(ptLayer, pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x1 - tOriginalPosition.x, y1 - tOriginalPosition.y, 1.0f}).xy), tOptions.uColor, (plVec2){s1, t1});
                pl__add_vertex(ptLayer, pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x0 - tOriginalPosition.x, y1 - tOriginalPosition.y, 1.0f}).xy), tOptions.uColor, (plVec2){s0, t1});

                pl__add_index(ptLayer, uVtxStart, 1, 0, 2);
                pl__add_index(ptLayer, uVtxStart, 2, 0, 3);
            }
        }
    }
}

static void
pl_add_text_clipped_ex(plDrawLayer2D* ptLayer, plVec2 p, const char* pcText, plVec2 tMin, plVec2 tMax, plDrawTextOptions tOptions)
{

    if(tOptions.pcTextEnd == NULL)
    {
        tOptions.pcTextEnd = pcText;
        while (*tOptions.pcTextEnd != '\0')
            tOptions.pcTextEnd++;
    }

    // const plVec2 tTextSize = pl_calculate_text_size_ex(font, size, text, pcTextEnd, wrap);
    const plRect tClipRect = {tMin, tMax};

    plFont* ptFont = tOptions.ptFont;
    const float fSize = tOptions.fSize == 0.0f ? ptFont->fSize : tOptions.fSize;
    const char* pcTextEnd = tOptions.pcTextEnd;

    const float fScale = fSize > 0.0f ? fSize / ptFont->fSize : 1.0f;

    float fLineSpacing = fScale * ptFont->_fLineSpacing;
    const plVec2 tOriginalPosition = p;
    bool bFirstCharacter = true;
    bool bNoTransform = tOptions.tTransform.x11 == 0;

    while(pcText < pcTextEnd)
    {
        uint32_t c = (uint32_t)*pcText;
        if(c < 0x80)
            pcText += 1;
        else
        {
            pcText += pl_text_char_from_utf8(&c, pcText, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            p.x = tOriginalPosition.x;
            p.y += fLineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else if(bNoTransform)
        {
            const plFontGlyph* ptGlyph = pl__find_glyph(ptFont, c);

            float x0,y0,s0,t0; // top-left
            float x1,y1,s1,t1; // bottom-right

            // adjust for left side bearing if first char
            if(bFirstCharacter)
            {
                if(ptGlyph->fLeftBearing > 0.0f)
                    p.x += ptGlyph->fLeftBearing * fScale;
                bFirstCharacter = false;
            }

            x0 = p.x + ptGlyph->x0 * fScale;
            x1 = p.x + ptGlyph->x1 * fScale;
            y0 = p.y + ptGlyph->y0 * fScale;
            y1 = p.y + ptGlyph->y1 * fScale;

            if(tOptions.fWrap > 0.0f && x1 > tOriginalPosition.x + tOptions.fWrap)
            {
                x0 = tOriginalPosition.x + ptGlyph->x0 * fScale;
                y0 = y0 + fLineSpacing;
                x1 = tOriginalPosition.x + ptGlyph->x1 * fScale;
                y1 = y1 + fLineSpacing;

                p.x = tOriginalPosition.x;
                p.y += fLineSpacing;
            }
            s0 = ptGlyph->u0;
            t0 = ptGlyph->v0;
            s1 = ptGlyph->u1;
            t1 = ptGlyph->v1;

            p.x += ptGlyph->fXAdvance * fScale;
            if(c != ' ' && pl_rect_contains_point(&tClipRect, p))
            {
                pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, (bool)ptGlyph->iSDF);
                pl__reserve_triangles(ptLayer, 6, 4);
                const uint32_t uVtxStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
                pl__add_vertex(ptLayer, (plVec2){x0, y0}, tOptions.uColor, (plVec2){s0, t0});
                pl__add_vertex(ptLayer, (plVec2){x1, y0}, tOptions.uColor, (plVec2){s1, t0});
                pl__add_vertex(ptLayer, (plVec2){x1, y1}, tOptions.uColor, (plVec2){s1, t1});
                pl__add_vertex(ptLayer, (plVec2){x0, y1}, tOptions.uColor, (plVec2){s0, t1});

                pl__add_index(ptLayer, uVtxStart, 1, 0, 2);
                pl__add_index(ptLayer, uVtxStart, 2, 0, 3);
            }
        }
        else
        {
            const plFontGlyph* ptGlyph = pl__find_glyph(ptFont, c);

            float x0,y0,s0,t0; // top-left
            float x1,y1,s1,t1; // bottom-right

            // adjust for left side bearing if first char
            if(bFirstCharacter)
            {
                if(ptGlyph->fLeftBearing > 0.0f)
                    p.x += ptGlyph->fLeftBearing * fScale;
                bFirstCharacter = false;
            }

            x0 = p.x + ptGlyph->x0 * fScale;
            x1 = p.x + ptGlyph->x1 * fScale;
            y0 = p.y + ptGlyph->y0 * fScale;
            y1 = p.y + ptGlyph->y1 * fScale;

            if(tOptions.fWrap > 0.0f && x1 > tOriginalPosition.x + tOptions.fWrap)
            {
                x0 = tOriginalPosition.x + ptGlyph->x0 * fScale;
                y0 = y0 + fLineSpacing;
                x1 = tOriginalPosition.x + ptGlyph->x1 * fScale;
                y1 = y1 + fLineSpacing;

                p.x = tOriginalPosition.x;
                p.y += fLineSpacing;
            }
            s0 = ptGlyph->u0;
            t0 = ptGlyph->v0;
            s1 = ptGlyph->u1;
            t1 = ptGlyph->v1;

            p.x += ptGlyph->fXAdvance * fScale;

            plVec2 tPoint1 = pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x0 - tOriginalPosition.x, y0 - tOriginalPosition.y, 1.0f}).xy);
            plVec2 tPoint2 = pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x1 - tOriginalPosition.x, y1 - tOriginalPosition.y, 1.0f}).xy);

            if(c != ' ' && pl_rect_contains_point(&tClipRect, tPoint1) && pl_rect_contains_point(&tClipRect, tPoint2))
            {
                pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, (bool)ptGlyph->iSDF);
                pl__reserve_triangles(ptLayer, 6, 4);
                const uint32_t uVtxStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
                pl__add_vertex(ptLayer, tPoint1, tOptions.uColor, (plVec2){s0, t0});
                pl__add_vertex(ptLayer, pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x1 - tOriginalPosition.x, y0 - tOriginalPosition.y, 1.0f}).xy), tOptions.uColor, (plVec2){s1, t0});
                pl__add_vertex(ptLayer, tPoint2, tOptions.uColor, (plVec2){s1, t1});
                pl__add_vertex(ptLayer, pl_add_vec2(tOriginalPosition, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){x0 - tOriginalPosition.x, y1 - tOriginalPosition.y, 1.0f}).xy), tOptions.uColor, (plVec2){s0, t1});

                pl__add_index(ptLayer, uVtxStart, 1, 0, 2);
                pl__add_index(ptLayer, uVtxStart, 2, 0, 3);
            }
        }
    }
}

static void
pl_add_triangle(plDrawLayer2D* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plDrawLineOptions tOptions)
{
    pl_sb_push(ptLayer->sbtPath, tP0);
    pl_sb_push(ptLayer->sbtPath, tP1);
    pl_sb_push(ptLayer->sbtPath, tP2);
    pl_sb_push(ptLayer->sbtPath, tP0);
    pl__submit_path(ptLayer, tOptions);    
}

static void
pl_add_triangle_filled(plDrawLayer2D* ptLayer, plVec2 p0, plVec2 p1, plVec2 p2, plDrawSolidOptions tOptions)
{
    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 3, 3);

    const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    pl__add_vertex(ptLayer, p0, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, p1, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, p2, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);

    pl__add_index(ptLayer, uVertexStart, 0, 1, 2);
}

static void
pl_add_triangles_filled(plDrawLayer2D* ptLayer, plVec2* atPoints, uint32_t uCount, plDrawSolidOptions tOptions)
{
    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 3 * uCount, 3 * uCount);

    for(uint32_t i = 0; i < uCount; i++)
    {
        const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
        pl__add_vertex(ptLayer, atPoints[i * 3 + 0], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
        pl__add_vertex(ptLayer, atPoints[i * 3 + 1], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
        pl__add_vertex(ptLayer, atPoints[i * 3 + 2], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
        pl__add_index(ptLayer, uVertexStart, 0, 1, 2);
    }
}

static void
pl_add_rect(plDrawLayer2D* ptLayer, plVec2 tMinP, plVec2 tMaxP, plDrawLineOptions tOptions)
{
    const plVec2 fBotLeftVec  = {tMinP.x, tMaxP.y};
    const plVec2 fTopRightVec = {tMaxP.x, tMinP.y};

    pl_sb_push(ptLayer->sbtPath, tMinP);
    pl_sb_push(ptLayer->sbtPath, fBotLeftVec);
    pl_sb_push(ptLayer->sbtPath, tMaxP);
    pl_sb_push(ptLayer->sbtPath, fTopRightVec);
    pl_sb_push(ptLayer->sbtPath, tMinP);
    pl__submit_path(ptLayer, tOptions);   
}

static void
pl_add_rect_filled(plDrawLayer2D* ptLayer, plVec2 tMinP, plVec2 tMaxP, plDrawSolidOptions tOptions)
{
    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 6, 4);

    const plVec2 tBottomLeft = { tMinP.x, tMaxP.y };
    const plVec2 tTopRight =   { tMaxP.x, tMinP.y };

    const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    pl__add_vertex(ptLayer, tMinP,       tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tBottomLeft, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tMaxP,       tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tTopRight,   tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);

    pl__add_index(ptLayer, uVertexStart, 0, 1, 2);
    pl__add_index(ptLayer, uVertexStart, 0, 2, 3);
}

static void
pl_add_rect_rounded_ex(
    plDrawLayer2D* ptLayer, plVec2 tMinP, plVec2 tMaxP, float fRadius,
    uint32_t uSegments, plDrawRectFlags tFlags, plDrawLineOptions tOptions)
{
    // segments is the number of segments used to approximate one corner

    if(fRadius <= 0.0f)
    {
        pl_add_rect(ptLayer, tMinP, tMaxP, tOptions);
        return;
    }
    else
    {
        if(tFlags == PL_DRAW_RECT_FLAG_NONE)
            tFlags = PL_DRAW_RECT_FLAG_ROUND_CORNERS_All;
    }

    if(uSegments == 0)
        uSegments = 4;

    const float fIncrement = PL_PI_2 / uSegments;
    float fTheta = 0.0f;

    const plVec2 tBottomRightStart = { tMaxP.x, tMaxP.y - fRadius };
    const plVec2 tBottomRightInner = { tMaxP.x - fRadius, tMaxP.y - fRadius };
    const plVec2 tBottomRightEnd   = { tMaxP.x - fRadius, tMaxP.y };

    const plVec2 tBottomLeftStart  = { tMinP.x + fRadius, tMaxP.y };
    const plVec2 tBottomLeftInner  = { tMinP.x + fRadius, tMaxP.y - fRadius };
    const plVec2 tBottomLeftEnd    = { tMinP.x , tMaxP.y - fRadius};
 
    const plVec2 tTopLeftStart     = { tMinP.x, tMinP.y + fRadius };
    const plVec2 tTopLeftInner     = { tMinP.x + fRadius, tMinP.y + fRadius };
    const plVec2 tTopLeftEnd       = { tMinP.x + fRadius, tMinP.y };

    const plVec2 tTopRightStart    = { tMaxP.x - fRadius, tMinP.y };
    const plVec2 tTopRightInner    = { tMaxP.x - fRadius, tMinP.y + fRadius };
    const plVec2 tTopRightEnd      = { tMaxP.x, tMinP.y + fRadius };
    
    pl_sb_push(ptLayer->sbtPath, tBottomRightStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbtPath, ((plVec2){tBottomRightInner.x + fRadius * sinf(fTheta + PL_PI_2), tBottomRightInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbtPath, tBottomRightEnd);

    pl_sb_push(ptLayer->sbtPath, tBottomLeftStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbtPath, ((plVec2){tBottomLeftInner.x + fRadius * sinf(fTheta + PL_PI_2), tBottomLeftInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbtPath, tBottomLeftEnd);

    pl_sb_push(ptLayer->sbtPath, tTopLeftStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbtPath, ((plVec2){tTopLeftInner.x + fRadius * sinf(fTheta + PL_PI_2), tTopLeftInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbtPath, tTopLeftEnd);

    pl_sb_push(ptLayer->sbtPath, tTopRightStart);
    fTheta += fIncrement;
    for(uint32_t i = 1; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbtPath, ((plVec2){tTopRightInner.x + fRadius * sinf(fTheta + PL_PI_2), tTopRightInner.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbtPath, tTopRightEnd);
    pl_sb_push(ptLayer->sbtPath, tBottomRightStart);
    pl__submit_path(ptLayer, tOptions);
}

static void
pl_add_rect_rounded_filled_ex(
        plDrawLayer2D* ptLayer, plVec2 tMinP, plVec2 tMaxP, float fRadius,
        uint32_t uSegments, plDrawRectFlags tFlags, plDrawSolidOptions tOptions)
{
    if(fRadius <= 0.0f)
    {
        pl_add_rect_filled(ptLayer, tMinP, tMaxP, tOptions);
        return;
    }
    else
    {
        if(tFlags == PL_DRAW_RECT_FLAG_NONE)
            tFlags = PL_DRAW_RECT_FLAG_ROUND_CORNERS_All;
    }

    if(tMaxP.x - tMinP.x < fRadius * 2.0f)
    {
        pl_add_rect_filled(ptLayer, tMinP, tMaxP, tOptions);
        return;
    }

    if(tMaxP.y - tMinP.y < fRadius * 2.0f)
    {
        pl_add_rect_filled(ptLayer, tMinP, tMaxP, tOptions);
        return;
    }

    if(uSegments == 0)
        uSegments = 4;

    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 30, 12);

    const plVec2 tInnerTopLeft = {tMinP.x + fRadius, tMinP.y + fRadius};
    const plVec2 tInnerBottomLeft = {tMinP.x + fRadius, tMaxP.y - fRadius};
    const plVec2 tInnerBottomRight = {tMaxP.x - fRadius, tMaxP.y - fRadius};
    const plVec2 tInnerTopRight = {tMaxP.x - fRadius, tMinP.y + fRadius};

    const plVec2 tOuterTopLeft0 = {tMinP.x + fRadius, tMinP.y};
    const plVec2 tOuterTopLeft1 = {tMinP.x, tMinP.y + fRadius};
    const plVec2 tOuterBottomLeft0 = {tMinP.x, tMaxP.y - fRadius};
    const plVec2 tOuterBottomLeft1 = {tMinP.x + fRadius, tMaxP.y};
    const plVec2 tOuterBottomRight0 = {tMaxP.x - fRadius, tMaxP.y};
    const plVec2 tOuterBottomRight1 = {tMaxP.x, tMaxP.y - fRadius};
    const plVec2 tOuterTopRight0 = {tMaxP.x, tMinP.y + fRadius};
    const plVec2 tOuterTopRight1 = {tMaxP.x - fRadius, tMinP.y};
    
    const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    pl__add_vertex(ptLayer, tInnerTopLeft, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tInnerBottomLeft, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tInnerBottomRight, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tInnerTopRight, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);

    pl__add_vertex(ptLayer, tOuterTopLeft1, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tOuterBottomLeft0, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tOuterBottomLeft1, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tOuterBottomRight0, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tOuterBottomRight1, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tOuterTopRight0, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tOuterTopRight1, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    pl__add_vertex(ptLayer, tOuterTopLeft0, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    
    // center
    pl__add_index(ptLayer, uVertexStart, 0, 1, 2);
    pl__add_index(ptLayer, uVertexStart, 0, 2, 3);

    // left
    pl__add_index(ptLayer, uVertexStart, 4, 5, 1);
    pl__add_index(ptLayer, uVertexStart, 4, 1, 0);

    // bottom
    pl__add_index(ptLayer, uVertexStart, 6, 7, 2);
    pl__add_index(ptLayer, uVertexStart, 6, 2, 1);

    // right
    pl__add_index(ptLayer, uVertexStart, 3, 2, 8);
    pl__add_index(ptLayer, uVertexStart, 3, 8, 9);

    // top
    pl__add_index(ptLayer, uVertexStart, 0, 3, 10);
    pl__add_index(ptLayer, uVertexStart, 0, 10, 11);

    const float fIncrement = PL_PI_2 / uSegments;
    float fTheta = PL_PI_2 + fIncrement;
    plVec2 tLastPoint = tOuterTopLeft0;

    if(tFlags & PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT)
    {
        for(uint32_t i = 0; i < uSegments - 1; i++)
        {
            plVec2 tPoint = {tInnerTopLeft.x + fRadius * cosf(fTheta), tInnerTopLeft.y - fRadius * sinf(fTheta)};
            pl_add_triangle_filled(ptLayer, tInnerTopLeft, tLastPoint, tPoint, tOptions);
            tLastPoint = tPoint;
            fTheta += fIncrement;
        }
        pl_add_triangle_filled(ptLayer, tInnerTopLeft, tLastPoint, tOuterTopLeft1, tOptions);
    }
    else
    {
        pl_add_triangle_filled(ptLayer, tInnerTopLeft, tOuterTopLeft0, tMinP, tOptions);
        pl_add_triangle_filled(ptLayer, tInnerTopLeft, tMinP, tOuterTopLeft1, tOptions);
    }

    if(tFlags & PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT)
    {
        fTheta = PL_PI + fIncrement;
        tLastPoint = tOuterBottomLeft0;
        for(uint32_t i = 0; i < uSegments - 1; i++)
        {
            plVec2 tPoint = {tInnerBottomLeft.x + fRadius * cosf(fTheta), tInnerBottomLeft.y - fRadius * sinf(fTheta)};
            pl_add_triangle_filled(ptLayer, tInnerBottomLeft, tLastPoint, tPoint, tOptions);
            tLastPoint = tPoint;
            fTheta += fIncrement;
        }
        pl_add_triangle_filled(ptLayer, tInnerBottomLeft, tLastPoint, tOuterBottomLeft1, tOptions);
    }
    else
    {
        pl_add_triangle_filled(ptLayer, tInnerBottomLeft, tOuterBottomLeft0, (plVec2){tMinP.x, tMaxP.y}, tOptions);
        pl_add_triangle_filled(ptLayer, tInnerBottomLeft, (plVec2){tMinP.x, tMaxP.y}, tOuterBottomLeft1, tOptions);
    }

    if(tFlags & PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT)
    {
        fTheta = PL_PI + PL_PI_2 + fIncrement;
        tLastPoint = tOuterBottomRight0;
        for(uint32_t i = 0; i < uSegments - 1; i++)
        {
            plVec2 tPoint = {tInnerBottomRight.x + fRadius * cosf(fTheta), tInnerBottomRight.y - fRadius * sinf(fTheta)};
            pl_add_triangle_filled(ptLayer, tInnerBottomRight, tLastPoint, tPoint, tOptions);
            tLastPoint = tPoint;
            fTheta += fIncrement;
        }
        pl_add_triangle_filled(ptLayer, tInnerBottomRight, tLastPoint, tOuterBottomRight1, tOptions);
    }
    else
    {
        pl_add_triangle_filled(ptLayer, tInnerBottomRight, tOuterBottomRight0, tMaxP, tOptions);
        pl_add_triangle_filled(ptLayer, tInnerBottomRight, tMaxP, tOuterBottomRight1, tOptions);
    }

    if(tFlags & PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT)
    {
        fTheta = fIncrement;
        tLastPoint = tOuterTopRight0;
        for(uint32_t i = 0; i < uSegments - 1; i++)
        {
            plVec2 tPoint = {tInnerTopRight.x + fRadius * cosf(fTheta), tInnerTopRight.y - fRadius * sinf(fTheta)};
            pl_add_triangle_filled(ptLayer, tInnerTopRight, tLastPoint, tPoint, tOptions);
            tLastPoint = tPoint;
            fTheta += fIncrement;
        }
        pl_add_triangle_filled(ptLayer, tInnerTopRight, tLastPoint, tOuterTopRight1, tOptions);
    }
    else
    {
        pl_add_triangle_filled(ptLayer, tInnerTopRight, tOuterTopRight0, (plVec2){tMaxP.x, tMinP.y}, tOptions);
        pl_add_triangle_filled(ptLayer, tInnerTopRight, (plVec2){tMaxP.x, tMinP.y}, tOuterTopRight1, tOptions);
    }
}

static  void
pl_add_quad(plDrawLayer2D* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plDrawLineOptions tOptions)
{
    pl_sb_push(ptLayer->sbtPath, tP0);
    pl_sb_push(ptLayer->sbtPath, tP1);
    pl_sb_push(ptLayer->sbtPath, tP2);
    pl_sb_push(ptLayer->sbtPath, tP3);
    pl_sb_push(ptLayer->sbtPath, tP0);
    pl__submit_path(ptLayer, tOptions);
}

static void
pl_add_quad_filled(plDrawLayer2D* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plDrawSolidOptions tOptions)
{
    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 6, 4);

    const uint32_t uVtxStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    pl__add_vertex(ptLayer, tP0, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv); // top left
    pl__add_vertex(ptLayer, tP1, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv); // bot left
    pl__add_vertex(ptLayer, tP2, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv); // bot right
    pl__add_vertex(ptLayer, tP3, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv); // top right

    pl__add_index(ptLayer, uVtxStart, 0, 1, 2);
    pl__add_index(ptLayer, uVtxStart, 0, 2, 3);
}

static void
pl_add_circle(plDrawLayer2D* ptLayer, plVec2 tP, float fRadius, uint32_t uSegments, plDrawLineOptions tOptions)
{
    if(uSegments == 0){ uSegments = 12; }
    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        pl_sb_push(ptLayer->sbtPath, ((plVec2){tP.x + fRadius * sinf(fTheta + PL_PI_2), tP.y + fRadius * sinf(fTheta)}));
        fTheta += fIncrement;
    }
    pl_sb_push(ptLayer->sbtPath, ((plVec2){tP.x + fRadius, tP.y}));
    pl__submit_path(ptLayer, tOptions);   
}

static void
pl_add_circle_filled(plDrawLayer2D* ptLayer, plVec2 tP, float fRadius, uint32_t uSegments, plDrawSolidOptions tOptions)
{
    if(uSegments == 0){ uSegments = 12; }
    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 3 * uSegments, uSegments + 1);

    const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    pl__add_vertex(ptLayer, tP, tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        pl__add_vertex(ptLayer,
            ((plVec2){tP.x + (fRadius * sinf(fTheta + PL_PI_2)), tP.y + (fRadius * sinf(fTheta))}),
            tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
        fTheta += fIncrement;
    }

    for(uint32_t i = 0; i < uSegments - 1; i++)
        pl__add_index(ptLayer, uVertexStart, i + 1, 0, i + 2);
    pl__add_index(ptLayer, uVertexStart, uSegments, 0, 1);
}

static void
pl_add_polygon(plDrawLayer2D* ptLayer, plVec2* tPoints, uint32_t uPointsSize, plDrawLineOptions tOptions)
{
    for(uint32_t i = 0; i < uPointsSize; i++)
    {
        pl_sb_push(ptLayer->sbtPath, tPoints[i]);
    }

    pl_sb_push(ptLayer->sbtPath, tPoints[0]);
    pl__submit_path(ptLayer, tOptions);
}

static void
pl_add_convex_polygon_filled(plDrawLayer2D* ptLayer, plVec2* tPoints, uint32_t uPointsSize, plDrawSolidOptions tOptions)
{
    pl__prepare_draw_command(ptLayer, gptDrawCtx->ptAtlas->tTexture, false);
    pl__reserve_triangles(ptLayer, 3 * (uPointsSize - 2), uPointsSize);

    const uint32_t uVtxStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    for(uint32_t i = 0; i < uPointsSize; i++)
    {
        pl__add_vertex(ptLayer, tPoints[i], tOptions.uColor, gptDrawCtx->ptAtlas->_tWhiteUv);
    }

    uint32_t numTriangles = uPointsSize - 2;
    for(uint32_t i = 0; i < numTriangles; i++)
    {
        pl__add_index(ptLayer, uVtxStart, 0, i + 1, i + 2);
    }
}

static void
pl_add_image_ex(plDrawLayer2D* ptLayer, plTextureID tTexture, plVec2 tPMin, plVec2 tPMax, plVec2 tUvMin, plVec2 tUvMax, uint32_t uColor)
{
    pl__prepare_draw_command(ptLayer, tTexture, false);
    pl__reserve_triangles(ptLayer, 6, 4);

    const plVec2 tBottomLeft = { tPMin.x, tPMax.y };
    const plVec2 tTopRight =   { tPMax.x, tPMin.y };

    const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    pl__add_vertex(ptLayer, tPMin,      uColor, tUvMin);
    pl__add_vertex(ptLayer, tBottomLeft, uColor, (plVec2){tUvMin.x, tUvMax.y});
    pl__add_vertex(ptLayer, tPMax,      uColor, tUvMax);
    pl__add_vertex(ptLayer, tTopRight,   uColor, (plVec2){tUvMax.x, tUvMin.y});

    pl__add_index(ptLayer, uVertexStart, 0, 1, 2);
    pl__add_index(ptLayer, uVertexStart, 0, 2, 3);
}

void
pl_add_image_quad_ex(plDrawLayer2D* ptLayer, plTextureID tTexture, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec2 tUv0, plVec2 tUv1, plVec2 tUv2, plVec2 tUv3, uint32_t uColor)
{
    pl__prepare_draw_command(ptLayer, tTexture, false);
    pl__reserve_triangles(ptLayer, 6, 4);


    const uint32_t uVertexStart = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer);
    pl__add_vertex(ptLayer, tP0, uColor, tUv0);
    pl__add_vertex(ptLayer, tP1, uColor, tUv1);
    pl__add_vertex(ptLayer, tP2, uColor, tUv2);
    pl__add_vertex(ptLayer, tP3, uColor, tUv3);

    pl__add_index(ptLayer, uVertexStart, 0, 1, 2);
    pl__add_index(ptLayer, uVertexStart, 0, 2, 3);
}

void
pl_add_image_quad(plDrawLayer2D* ptLayer, plTextureID tTexture, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3)
{
    pl_add_image_quad_ex(ptLayer, tTexture, tP0, tP1, tP2, tP3, (plVec2){0}, (plVec2){0.0f, 1.0f}, (plVec2){1.0f, 1.0f}, (plVec2){1.0f, 0.0f}, PL_COLOR_32_WHITE);
}

static void
pl_add_image(plDrawLayer2D* ptLayer, plTextureID tTexture, plVec2 tPMin, plVec2 tPMax)
{
    pl_add_image_ex(ptLayer, tTexture, tPMin, tPMax, (plVec2){0}, (plVec2){1.0f, 1.0f}, PL_COLOR_32_WHITE);
}

static void
pl_add_bezier_quad(plDrawLayer2D* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, uint32_t uSegments, plDrawLineOptions tOptions)
{
    // order of the bezier curve inputs are 0=start, 1=control, 2=ending

    if(uSegments == 0)
        uSegments = 12;

    // push first point
    pl_sb_push(ptLayer->sbtPath, tP0);

    // calculate and push points between first and last
    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        
        const plVec2 p0 = pl_mul_vec2_scalarf(tP0, uu);
        const plVec2 p1 = pl_mul_vec2_scalarf(tP1, (2.0f * u * t)); 
        const plVec2 p2 = pl_mul_vec2_scalarf(tP2, tt); 
        const plVec2 p3 = pl_add_vec2(p0,p1);
        const plVec2 p4 = pl_add_vec2(p2,p3);

        pl_sb_push(ptLayer->sbtPath, p4);
    }

    // push last point
    pl_sb_push(ptLayer->sbtPath, tP2);
    pl__submit_path(ptLayer, tOptions); 
}

static void
pl_add_bezier_cubic(plDrawLayer2D* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, uint32_t uSegments, plDrawLineOptions tOptions)
{
    // order of the bezier curve inputs are 0=start, 1=control 1, 2=control 2, 3=ending
    if(uSegments == 0)
        uSegments = 12;

    // push first point
    pl_sb_push(ptLayer->sbtPath, tP0);

    // calculate and push points between first and last
    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        const float uuu = uu * u;
        const float ttt = tt * t;
        
        const plVec2 p0 = pl_mul_vec2_scalarf(tP0, uuu);
        const plVec2 p1 = pl_mul_vec2_scalarf(tP1, (3.0f * uu * t)); 
        const plVec2 p2 = pl_mul_vec2_scalarf(tP2, (3.0f * u * tt)); 
        const plVec2 p3 = pl_mul_vec2_scalarf(tP3, (ttt));
        const plVec2 p5 = pl_add_vec2(p0,p1);
        const plVec2 p6 = pl_add_vec2(p2,p3);
        const plVec2 p7 = pl_add_vec2(p5,p6);

        pl_sb_push(ptLayer->sbtPath, p7);
    }

    // push last point
    pl_sb_push(ptLayer->sbtPath, tP3);
    pl__submit_path(ptLayer, tOptions); 
}

static plFont*
pl_add_font_from_memory_ttf(plFontAtlas* ptAtlas, plFontConfig tConfig, void* pData)
{
    ptAtlas->_iGlyphPadding = 1;

    plFont* ptFont = NULL;
    if(tConfig.ptMergeFont)
        ptFont = tConfig.ptMergeFont;
    else
    {
        ptFont = PL_ALLOC(sizeof(plFont));
        memset(ptFont, 0, sizeof(plFont));
        ptFont->_fLineSpacing = 0.0f;
        ptFont->fSize = tConfig.fSize;
        ptFont->_ptNextFont = ptAtlas->_ptFontListHead;
        ptAtlas->_ptFontListHead = ptFont;
    }
    const uint32_t uConfigIndex = pl_sb_size(ptFont->_sbtConfigs);
    const uint32_t uPrepIndex = uConfigIndex;
    pl_sb_add(ptFont->_sbtConfigs);
    pl_sb_push(ptFont->_sbtPreps, (plFontPrepData){0});

    plFontPrepData* ptPrep = &ptFont->_sbtPreps[uPrepIndex];
    stbtt_InitFont(&ptPrep->tFontInfo, (unsigned char*)pData, 0);

    // prepare stb
    
    // get vertical font metrics
    int fAscent = 0;
    int fDescent = 0;
    int fLineGap = 0;
    stbtt_GetFontVMetrics(&ptPrep->tFontInfo, &fAscent, &fDescent, &fLineGap);

    // calculate scaling factor
    ptPrep->fScale = 1.0f;
    if(ptFont->fSize > 0)
        ptPrep->fScale = stbtt_ScaleForPixelHeight(&ptPrep->tFontInfo, ptFont->fSize);
    else
        ptPrep->fScale = stbtt_ScaleForMappingEmToPixels(&ptPrep->tFontInfo, -ptFont->fSize);

    // calculate SDF pixel increment
    if(tConfig.bSdf)
        tConfig._fSdfPixelDistScale = (float)tConfig.ucOnEdgeValue / (float) tConfig.iSdfPadding;

    // calculate base line spacing
    ptPrep->fAscent = ceilf(fAscent * ptPrep->fScale);
    ptPrep->fDescent = floorf(fDescent * ptPrep->fScale);
    ptFont->_fLineSpacing = pl_max(ptFont->_fLineSpacing, (ptPrep->fAscent - ptPrep->fDescent + ptPrep->fScale * (float)fLineGap));

    // convert individual chars to ranges
    for(uint32_t i = 0; i < tConfig.uRangeCount; i++)
    {
        pl_sb_push(tConfig._sbtRanges, tConfig.ptRanges[i]);
    }

    // convert individual chars to ranges
    for(uint32_t i = 0; i < tConfig.uIndividualCharCount; i++)
    {
        plFontRange tRange = {
            .uCharCount      = 1,
            .iFirstCodePoint = tConfig.piIndividualChars[i],
            ._uConfigIndex   = uConfigIndex
        };
        pl_sb_push(tConfig._sbtRanges, tRange);
    }

    // find total number of glyphs/chars required
    // const uint32_t uGlyphOffset = pl_sb_size(ptFont->_sbtGlyphs);
    uint32_t uTotalCharCount = 0u;
    for(uint32_t i = 0; i < pl_sb_size(tConfig._sbtRanges); i++)
    {
        uTotalCharCount += tConfig._sbtRanges[i].uCharCount;
        uTotalCharCount += tConfig._sbtRanges[i]._uConfigIndex = uConfigIndex;
    }
    
    pl_sb_reserve(ptFont->_sbtGlyphs, pl_sb_size(ptFont->_sbtGlyphs) + uTotalCharCount);
    pl_sb_resize(tConfig._sbtCharData, uTotalCharCount);

    if(tConfig.bSdf)
    {
        pl_sb_reserve(ptAtlas->_sbtCustomRects, pl_sb_size(ptAtlas->_sbtCustomRects) + uTotalCharCount); // is this correct
    }

    ptPrep->ptRanges = PL_ALLOC(sizeof(stbtt_pack_range) * pl_sb_size(tConfig._sbtRanges));
    memset(ptPrep->ptRanges, 0, sizeof(stbtt_pack_range) * pl_sb_size(tConfig._sbtRanges));

    // find max codepoint & set range pointers into font char data
    int k = 0;
    int iMaxCodePoint = 0;
    uTotalCharCount = 0u;
    bool bMissingGlyphAdded = false;

    for(uint32_t i = 0; i < pl_sb_size(tConfig._sbtRanges); i++)
    {
        plFontRange* ptRange = &tConfig._sbtRanges[i];
        ptRange->_uConfigIndex = uConfigIndex;
        ptPrep->uTotalCharCount += ptRange->uCharCount;
        pl_sb_push(ptFont->_sbtRanges, *ptRange);
    }

    if(!tConfig.bSdf)
    {
        ptPrep->ptRects = PL_ALLOC(sizeof(stbrp_rect) * ptPrep->uTotalCharCount);
    }

    for(uint32_t i = 0; i < pl_sb_size(tConfig._sbtRanges); i++)
    {
        plFontRange* ptRange = &tConfig._sbtRanges[i];

        if(ptRange->iFirstCodePoint + (int)ptRange->uCharCount > iMaxCodePoint)
            iMaxCodePoint = ptRange->iFirstCodePoint + (int)ptRange->uCharCount;

        // prepare stb stuff
        ptPrep->ptRanges[i].font_size = tConfig.fSize;
        ptPrep->ptRanges[i].first_unicode_codepoint_in_range = ptRange->iFirstCodePoint;
        ptPrep->ptRanges[i].chardata_for_range = (stbtt_packedchar*)&tConfig._sbtCharData[uTotalCharCount];
        ptPrep->ptRanges[i].num_chars = ptRange->uCharCount;
        ptPrep->ptRanges[i].h_oversample = (unsigned char) tConfig.uHOverSampling;
        ptPrep->ptRanges[i].v_oversample = (unsigned char) tConfig.uVOverSampling;

        // flag all characters as NOT packed
        memset(ptPrep->ptRanges[i].chardata_for_range, 0, sizeof(stbtt_packedchar) * ptRange->uCharCount);

        if(tConfig.bSdf)
        {
            for (uint32_t j = 0; j < (uint32_t)ptPrep->ptRanges[i].num_chars; j++) 
            {
                int iCodePoint = 0;
                if(ptPrep->ptRanges[i].array_of_unicode_codepoints)
                    iCodePoint = ptPrep->ptRanges[i].array_of_unicode_codepoints[j];
                else
                    iCodePoint = ptPrep->ptRanges[i].first_unicode_codepoint_in_range + j;


                int iWidth = 0;
                int iHeight = 0;
                int iXOff = 0;
                int iYOff = 0;
                unsigned char* pucBytes = stbtt_GetCodepointSDF(&ptPrep->tFontInfo,
                        stbtt_ScaleForPixelHeight(&ptPrep->tFontInfo, tConfig.fSize),
                        iCodePoint,
                        tConfig.iSdfPadding,
                        tConfig.ucOnEdgeValue,
                        tConfig._fSdfPixelDistScale,
                        &iWidth, &iHeight, &iXOff, &iYOff);

                int xAdvance = 0u;
                stbtt_GetCodepointHMetrics(&ptPrep->tFontInfo, iCodePoint, &xAdvance, NULL);

                tConfig._sbtCharData[uTotalCharCount + j].xOff = (float)(iXOff);
                tConfig._sbtCharData[uTotalCharCount + j].yOff = (float)(iYOff);
                tConfig._sbtCharData[uTotalCharCount + j].xOff2 = (float)(iXOff + iWidth);
                tConfig._sbtCharData[uTotalCharCount + j].yOff2 = (float)(iYOff + iHeight);
                tConfig._sbtCharData[uTotalCharCount + j].xAdv = ptPrep->fScale * (float)xAdvance;

                plFontCustomRect tCustomRect = {
                    .uWidth   = (uint32_t)iWidth,
                    .uHeight  = (uint32_t)iHeight,
                    .pucBytes = pucBytes
                };
                pl_sb_push(ptAtlas->_sbtCustomRects, tCustomRect);
                ptAtlas->_fTotalArea += iWidth * iHeight;
                
            }
            k += ptPrep->ptRanges[i].num_chars;
        }
        else // regular font
        {
            for(uint32_t j = 0; j < ptRange->uCharCount; j++)
            {
                int iCodepoint = 0;
                if(ptPrep->ptRanges[i].array_of_unicode_codepoints)
                    iCodepoint = ptPrep->ptRanges[i].array_of_unicode_codepoints[j];
                else
                    iCodepoint = ptPrep->ptRanges[i].first_unicode_codepoint_in_range + j;

                // bitmap
                int iGlyphIndex = stbtt_FindGlyphIndex(&ptPrep->tFontInfo, iCodepoint);
                if(iGlyphIndex == 0 && bMissingGlyphAdded)
                    ptPrep->ptRects[k].w = ptPrep->ptRects[k].h = 0;
                else
                {
                    int x0 = 0;
                    int y0 = 0;
                    int x1 = 0;
                    int y1 = 0;
                    stbtt_GetGlyphBitmapBoxSubpixel(&ptPrep->tFontInfo, iGlyphIndex,
                                                    ptPrep->fScale * tConfig.uHOverSampling,
                                                    ptPrep->fScale * tConfig.uVOverSampling,
                                                    0, 0, &x0, &y0, &x1, &y1);
                    ptPrep->ptRects[k].w = (stbrp_coord)(x1 - x0 + ptAtlas->_iGlyphPadding + tConfig.uHOverSampling - 1);
                    ptPrep->ptRects[k].h = (stbrp_coord)(y1 - y0 + ptAtlas->_iGlyphPadding + tConfig.uVOverSampling - 1);
                    ptAtlas->_fTotalArea += ptPrep->ptRects[k].w * ptPrep->ptRects[k].h;
                    if(iGlyphIndex == 0)
                        bMissingGlyphAdded = true; 
                }
                k++;
            }
        }
        uTotalCharCount += ptRange->uCharCount;
    }
    if(ptFont->_uCodePointCount == 0)
    {
        ptFont->_auCodePoints = PL_ALLOC(sizeof(uint32_t) * (uint32_t)iMaxCodePoint);
        ptFont->_uCodePointCount = (uint32_t)iMaxCodePoint;
    }
    else
    {
        uint32_t* puOldCodePoints = ptFont->_auCodePoints;
        ptFont->_auCodePoints = PL_ALLOC(sizeof(uint32_t) * ((uint32_t)iMaxCodePoint + ptFont->_uCodePointCount));
        memcpy(ptFont->_auCodePoints, puOldCodePoints, ptFont->_uCodePointCount * sizeof(uint32_t));
        ptFont->_uCodePointCount += (uint32_t)iMaxCodePoint;
        PL_FREE(puOldCodePoints);
    }
    ptFont->_sbtConfigs[uConfigIndex] = tConfig;
    return ptFont;
}

static plFont*
pl_add_font_from_file_ttf(plFontAtlas* ptAtlas, plFontConfig tConfig, const char* pcFile)
{
    size_t szFileSize = gptVfs->get_file_size_str(pcFile);
    plVfsFileHandle tHandle = gptVfs->open_file(pcFile, PL_VFS_FILE_MODE_READ);
    if(szFileSize)
    {
        
        uint8_t* puData = PL_ALLOC(szFileSize);
        memset(puData, 0, szFileSize);
        gptVfs->read_file(tHandle, puData, &szFileSize);
        gptVfs->close_file(tHandle);

        plFont* ptFont = pl_add_font_from_memory_ttf(ptAtlas, tConfig, puData);
        return ptFont;
    }
    return NULL;
}

static plVec2
pl_calculate_text_size(const char* pcText, plDrawTextOptions tOptions)
{

    if(tOptions.pcTextEnd == NULL)
    {
        tOptions.pcTextEnd = pcText;
        while(*tOptions.pcTextEnd != '\0')
            tOptions.pcTextEnd++;
    }
    
    plVec2 tResult = {0};
    plVec2 tCursor = {0};

    plFont* ptFont = tOptions.ptFont;
    const float fSize = tOptions.fSize == 0.0f ? ptFont->fSize : tOptions.fSize;
    const char* pcTextEnd = tOptions.pcTextEnd;

    const float fScale = fSize > 0.0f ? fSize / ptFont->fSize : 1.0f;

    const float fLineSpacing = fScale * ptFont->_fLineSpacing;
    plVec2 tOriginalPosition = {FLT_MAX, FLT_MAX};
    bool bFirstCharacter = true;
    bool bNoTransform = tOptions.tTransform.x11 == 0;

    while(pcText < pcTextEnd)
    {
        uint32_t c = (uint32_t)*pcText;
        if(c < 0x80)
            pcText += 1;
        else
        {
            pcText += pl_text_char_from_utf8(&c, pcText, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            tCursor.x = tOriginalPosition.x;
            tCursor.y += fLineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else
        {

            const plFontGlyph* ptGlyph = pl__find_glyph(ptFont, c);

            float x0,y0,s0,t0; // top-left
            float x1,y1,s1,t1; // bottom-right

            // adjust for left side bearing if first char
            if(bFirstCharacter)
            {
                if(ptGlyph->fLeftBearing > 0.0f)
                    tCursor.x += ptGlyph->fLeftBearing * fScale;
                bFirstCharacter = false;
                tOriginalPosition.x = tCursor.x + ptGlyph->x0 * fScale;
                tOriginalPosition.y = tCursor.y + ptGlyph->y0 * fScale;
            }

            x0 = tCursor.x + ptGlyph->x0 * fScale;
            x1 = tCursor.x + ptGlyph->x1 * fScale;
            y0 = tCursor.y + ptGlyph->y0 * fScale;
            y1 = tCursor.y + ptGlyph->y1 * fScale;

            if(tOptions.fWrap > 0.0f && x1 > tOriginalPosition.x + tOptions.fWrap)
            {
                x0 = tOriginalPosition.x + ptGlyph->x0 * fScale;
                y0 = y0 + fLineSpacing;
                x1 = tOriginalPosition.x + ptGlyph->x1 * fScale;
                y1 = y1 + fLineSpacing;

                tCursor.x = tOriginalPosition.x;
                tCursor.y += fLineSpacing;
            }

            if(x0 < tOriginalPosition.x)
                tOriginalPosition.x = x0;
            if(y0 < tOriginalPosition.y)
                tOriginalPosition.y = y0;

            s0 = ptGlyph->u0;
            t0 = ptGlyph->v0;
            s1 = ptGlyph->u1;
            t1 = ptGlyph->v1;

            if(x1 > tResult.x)
                tResult.x = x1;
            if(y1 > tResult.y)
                tResult.y = y1;

            tCursor.x += ptGlyph->fXAdvance * fScale;
        }   
    }
    plVec2 tTextSize = pl_sub_vec2(tResult, tOriginalPosition);
    if(bNoTransform)
        return tTextSize;
    else
    {
        plVec2 tOriginalPosition2 = pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){0.0f, 0.0f, 1.0f}).xy;
        plVec2 tMin = pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){tTextSize.x, 0.0f, 1.0f}).xy;
        // plVec2 tMid = pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){tTextSize.x, tTextSize.y * 0.5f, 1.0f}).xy;
        plVec2 tMax = pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){tTextSize.x, tTextSize.y, 1.0f}).xy;

        float fLength = pl_length_vec2(pl_sub_vec2(tMin, tOriginalPosition2));
        float fHeight = pl_length_vec2(pl_sub_vec2(tMax, tMin));
        return (plVec2){fLength, fHeight};
    }
}

static plRect
pl_calculate_text_bb(plVec2 tP, const char* pcText, plDrawTextOptions tOptions)
{
    if(tOptions.pcTextEnd == NULL)
    {
        tOptions.pcTextEnd = pcText;
        while(*tOptions.pcTextEnd != '\0')
            tOptions.pcTextEnd++;
    }

    plVec2 tTextSize = {0};
    plVec2 tCursor = {0};

    plFont* ptFont = tOptions.ptFont;
    const float fSize = tOptions.fSize == 0.0f ? ptFont->fSize : tOptions.fSize;
    const char* pcTextEnd = tOptions.pcTextEnd;

    const float fScale = fSize > 0.0f ? fSize / ptFont->fSize : 1.0f;

    float fLineSpacing = fScale * ptFont->_fLineSpacing;
    const plVec2 tOriginalPosition2 = tP;
    plVec2 tOriginalPosition = {FLT_MAX, FLT_MAX};
    bool bFirstCharacter = true;
    bool bNoTransform = tOptions.tTransform.x11 == 0;

    while(pcText < pcTextEnd)
    {
        uint32_t c = (uint32_t)*pcText;
        if(c < 0x80)
            pcText += 1;
        else
        {
            pcText += pl_text_char_from_utf8(&c, pcText, NULL);
            if(c == 0) // malformed UTF-8?
                break;
        }

        if(c == '\n')
        {
            tCursor.x = tOriginalPosition.x;
            tCursor.y += fLineSpacing;
        }
        else if(c == '\r')
        {
            // do nothing
        }
        else
        {

            const plFontGlyph* ptGlyph = pl__find_glyph(ptFont, c);

            float x0,y0,s0,t0; // top-left
            float x1,y1,s1,t1; // bottom-right

            // adjust for left side bearing if first char
            if(bFirstCharacter)
            {
                if(ptGlyph->fLeftBearing > 0.0f)
                    tCursor.x += ptGlyph->fLeftBearing * fScale;
                bFirstCharacter = false;
                tOriginalPosition.x = tCursor.x + ptGlyph->x0 * fScale;
                tOriginalPosition.y = tCursor.y + ptGlyph->y0 * fScale;
            }

            x0 = tCursor.x + ptGlyph->x0 * fScale;
            x1 = tCursor.x + ptGlyph->x1 * fScale;
            y0 = tCursor.y + ptGlyph->y0 * fScale;
            y1 = tCursor.y + ptGlyph->y1 * fScale;

            if(tOptions.fWrap > 0.0f && x1 > tOriginalPosition.x + tOptions.fWrap)
            {
                x0 = tOriginalPosition.x + ptGlyph->x0 * fScale;
                y0 = y0 + fLineSpacing;
                x1 = tOriginalPosition.x + ptGlyph->x1 * fScale;
                y1 = y1 + fLineSpacing;

                tCursor.x = tOriginalPosition.x;
                tCursor.y += fLineSpacing;
            }

            if(x0 < tOriginalPosition.x)
                tOriginalPosition.x = x0;
            if(y0 < tOriginalPosition.y)
                tOriginalPosition.y = y0;

            s0 = ptGlyph->u0;
            t0 = ptGlyph->v0;
            s1 = ptGlyph->u1;
            t1 = ptGlyph->v1;

            if(x1 > tTextSize.x)
                tTextSize.x = x1;
            if(y1 > tTextSize.y)
                tTextSize.y = y1;

            tCursor.x += ptGlyph->fXAdvance * fScale;
        }
    }

    tTextSize = pl_sub_vec2(tTextSize, tOriginalPosition);
    const plVec2 tStartOffset = pl_add_vec2(tP, tOriginalPosition);
    plRect tResult = pl_calculate_rect(tStartOffset, tTextSize);
    if(!bNoTransform)
    {

        plVec2 tTopLeft = pl_rect_top_left(&tResult);                             
        plVec2 tTopRight = pl_rect_top_right(&tResult);
        plVec2 tBottomLeft = pl_rect_bottom_left(&tResult);
        plVec2 tBottomRight = pl_rect_bottom_right(&tResult);    

        tTopLeft = pl_add_vec2(tOriginalPosition2, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){tTopLeft.x - tOriginalPosition2.x, tTopLeft.y - tOriginalPosition2.y, 1.0f}).xy);
        tTopRight = pl_add_vec2(tOriginalPosition2, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){tTopRight.x - tOriginalPosition2.x, tTopRight.y - tOriginalPosition2.y, 1.0f}).xy);
        tBottomLeft = pl_add_vec2(tOriginalPosition2, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){tBottomLeft.x - tOriginalPosition2.x, tBottomLeft.y - tOriginalPosition2.y, 1.0f}).xy);
        tBottomRight = pl_add_vec2(tOriginalPosition2, pl_mul_mat3_vec3(&tOptions.tTransform, (plVec3){tBottomRight.x - tOriginalPosition2.x, tBottomRight.y - tOriginalPosition2.y, 1.0f}).xy);
        
        tResult.tMin = tTopLeft;
        tResult.tMax = tTopLeft;

        if(tTopRight.x < tResult.tMin.x)    tResult.tMin.x = tTopRight.x;
        if(tTopRight.x > tResult.tMax.x)    tResult.tMax.x = tTopRight.x;
        if(tBottomLeft.x < tResult.tMin.x)  tResult.tMin.x = tBottomLeft.x;
        if(tBottomLeft.x > tResult.tMax.x)  tResult.tMax.x = tBottomLeft.x;
        if(tBottomRight.x < tResult.tMin.x) tResult.tMin.x = tBottomRight.x;
        if(tBottomRight.x > tResult.tMax.x) tResult.tMax.x = tBottomRight.x;

        if(tTopRight.y < tResult.tMin.y)    tResult.tMin.y = tTopRight.y;
        if(tTopRight.y > tResult.tMax.y)    tResult.tMax.y = tTopRight.y;
        if(tBottomLeft.y < tResult.tMin.y)  tResult.tMin.y = tBottomLeft.y;
        if(tBottomLeft.y > tResult.tMax.y)  tResult.tMax.y = tBottomLeft.y;
        if(tBottomRight.y < tResult.tMin.y) tResult.tMin.y = tBottomRight.y;
        if(tBottomRight.y > tResult.tMax.y) tResult.tMax.y = tBottomRight.y;
    }
    return tResult;
}

static void
pl_push_clip_rect_pt(plDrawList2D* ptDrawlist, const plRect* ptRect, bool bAccumulate)
{
    plRect tRect = *ptRect;
    if(bAccumulate && pl_sb_size(ptDrawlist->_sbtClipStack) > 0)
        tRect = pl_rect_clip_full(&tRect, &pl_sb_back(ptDrawlist->_sbtClipStack));
    pl_sb_push(ptDrawlist->_sbtClipStack, tRect);
}

static void
pl_push_clip_rect(plDrawList2D* ptDrawlist, plRect tRect, bool bAccumulate)
{
    if(bAccumulate && pl_sb_size(ptDrawlist->_sbtClipStack) > 0)
        tRect = pl_rect_clip_full(&tRect, &pl_sb_back(ptDrawlist->_sbtClipStack));
    pl_sb_push(ptDrawlist->_sbtClipStack, tRect);
}

static void
pl_pop_clip_rect(plDrawList2D* ptDrawlist)
{
    pl_sb_pop(ptDrawlist->_sbtClipStack);
}

static const plRect*
pl_get_clip_rect(plDrawList2D* ptDrawlist)
{
     if(pl_sb_size(ptDrawlist->_sbtClipStack) > 0)
        return &pl_sb_back(ptDrawlist->_sbtClipStack);
    return NULL;
}

static plFontAtlas*
pl_create_font_atlas(void)
{
    plFontAtlas* ptAtlas = PL_ALLOC(sizeof(plFontAtlas));
    memset(ptAtlas, 0, sizeof(plFontAtlas));
    return ptAtlas;
}

static void
pl_set_font_atlas(plFontAtlas* ptAtlas)
{
    gptDrawCtx->ptAtlas = ptAtlas;
}

static plFontAtlas*
pl_get_font_atlas(void)
{
    return gptDrawCtx->ptAtlas;
}

static plFont*
pl_get_first_font(plFontAtlas* ptAtlas)
{
    plFont* ptFont = ptAtlas->_ptFontListHead;
    while(ptFont)
    {
        if(ptFont->_ptNextFont)
            ptFont = ptFont->_ptNextFont;
        else
            break;
    }
    return ptFont;
}

static bool
pl_prepare_font_atlas(plFontAtlas* ptAtlas)
{

    // create our white location
    plFontCustomRect ptWhiteRect = {
        .uWidth = 8u,
        .uHeight = 8u,
        .uX = 0u,
        .uY = 0u,
        .pucBytes = malloc(64)
    };
    memset(ptWhiteRect.pucBytes, 255, 64);
    pl_sb_push(ptAtlas->_sbtCustomRects, ptWhiteRect);
    ptAtlas->_ptWhiteRect = &pl_sb_back(ptAtlas->_sbtCustomRects);
    ptAtlas->_fTotalArea += 64;

    // calculate final texture area required
    const float fTotalAtlasAreaSqrt = (float)sqrt((float)ptAtlas->_fTotalArea) + 1.0f;
    ptAtlas->tAtlasSize.x = 512;
    ptAtlas->tAtlasSize.y = 0;
    if(fTotalAtlasAreaSqrt >= 4096 * 0.7f)
        ptAtlas->tAtlasSize.x = 4096;
    else if(fTotalAtlasAreaSqrt >= 2048 * 0.7f)
        ptAtlas->tAtlasSize.x = 2048;
    else if(fTotalAtlasAreaSqrt >= 1024 * 0.7f)
        ptAtlas->tAtlasSize.x = 1024;

    // begin packing
    stbtt_pack_context tSpc = {0};
    stbtt_PackBegin(&tSpc, NULL, (uint32_t)ptAtlas->tAtlasSize.x, 1024 * 32, 0, ptAtlas->_iGlyphPadding, NULL);

    // allocate SDF rects
    stbrp_rect* ptRects = PL_ALLOC(pl_sb_size(ptAtlas->_sbtCustomRects) * sizeof(stbrp_rect));
    memset(ptRects, 0, sizeof(stbrp_rect) * pl_sb_size(ptAtlas->_sbtCustomRects));

    // transfer our data to stb data
    for(uint32_t i = 0; i < pl_sb_size(ptAtlas->_sbtCustomRects); i++)
    {
        ptRects[i].w = (int)ptAtlas->_sbtCustomRects[i].uWidth;
        ptRects[i].h = (int)ptAtlas->_sbtCustomRects[i].uHeight;
    }
    
    // pack bitmap fonts
    plFont* ptFont = ptAtlas->_ptFontListHead;
    while(ptFont)
    {
        const uint32_t uRangeCount = pl_sb_size(ptFont->_sbtRanges);
        for(uint32_t j = 0; j < uRangeCount; j++)
        {
            plFontRange* ptRange = &ptFont->_sbtRanges[j];
            if(!ptFont->_sbtConfigs[ptRange->_uConfigIndex].bSdf)
            {
                plFontPrepData* ptPrep = &ptFont->_sbtPreps[ptRange->_uConfigIndex];
                if(!ptPrep->bPrepped)
                {
                    stbtt_PackSetOversampling(&tSpc, ptFont->_sbtConfigs[ptRange->_uConfigIndex].uHOverSampling,
                        ptFont->_sbtConfigs[ptRange->_uConfigIndex].uVOverSampling);
                    stbrp_pack_rects((stbrp_context*)tSpc.pack_info, ptPrep->ptRects, ptPrep->uTotalCharCount);
                    for(uint32_t k = 0; k < ptPrep->uTotalCharCount; k++)
                    {
                        if(ptPrep->ptRects[k].was_packed)
                            ptAtlas->tAtlasSize.y = pl_max((float)ptAtlas->tAtlasSize.y, (float)(ptPrep->ptRects[k].y + ptPrep->ptRects[k].h));
                    }
                    ptPrep->bPrepped = true;
                }
            }
        }
        ptFont = ptFont->_ptNextFont;
    }

    // pack SDF fonts
    stbtt_PackSetOversampling(&tSpc, 1, 1);
    stbrp_pack_rects((stbrp_context*)tSpc.pack_info, ptRects, pl_sb_size(ptAtlas->_sbtCustomRects));

    const uint32_t uCustomRectCount = pl_sb_size(ptAtlas->_sbtCustomRects);
    for(uint32_t i = 0; i < uCustomRectCount; i++)
    {
        if(ptRects[i].was_packed)
            ptAtlas->tAtlasSize.y = pl_max((float)ptAtlas->tAtlasSize.y, (float)(ptRects[i].y + ptRects[i].h));
    }

    // grow cpu side buffers if needed
    if(ptAtlas->_szPixelDataSize < ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y)
    {
        if(ptAtlas->_pucPixelsAsAlpha8)
        {
            PL_FREE(ptAtlas->_pucPixelsAsAlpha8);
        }
        if(ptAtlas->pucPixelsAsRGBA32)
        {
            PL_FREE(ptAtlas->pucPixelsAsRGBA32);
        }

        ptAtlas->_pucPixelsAsAlpha8 = PL_ALLOC((uint32_t)(ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y));   
        ptAtlas->pucPixelsAsRGBA32 = PL_ALLOC((uint32_t)(ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y * 4));

        memset(ptAtlas->_pucPixelsAsAlpha8, 0, (uint32_t)(ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y));
        memset(ptAtlas->pucPixelsAsRGBA32, 0, (uint32_t)(ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y * 4));
    }
    tSpc.pixels = ptAtlas->_pucPixelsAsAlpha8;
    ptAtlas->_szPixelDataSize = (size_t)(ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y);

    // rasterize bitmap fonts
    ptFont = ptAtlas->_ptFontListHead;
    while(ptFont)
    {
        const uint32_t uConfigCount = pl_sb_size(ptFont->_sbtConfigs);
        for(uint32_t j = 0; j < uConfigCount; j++)
        {
            plFontPrepData* ptPrep = &ptFont->_sbtPreps[j];
            if(!ptFont->_sbtConfigs[j].bSdf)
                stbtt_PackFontRangesRenderIntoRects(&tSpc, &ptPrep->tFontInfo, ptPrep->ptRanges,
                    pl_sb_size(ptFont->_sbtConfigs[j]._sbtRanges), ptPrep->ptRects);
        }
        ptFont = ptFont->_ptNextFont;
    }

    // update SDF/custom data
    for(uint32_t i = 0; i < uCustomRectCount; i++)
    {
        ptAtlas->_sbtCustomRects[i].uX = (uint32_t)ptRects[i].x;
        ptAtlas->_sbtCustomRects[i].uY = (uint32_t)ptRects[i].y;
    }

    ptFont = ptAtlas->_ptFontListHead;
    while(ptFont)
    {
        uint32_t uCharDataOffset = 0;
        const uint32_t uConfigCount = pl_sb_size(ptFont->_sbtConfigs);
        for(uint32_t j = 0; j < uConfigCount; j++)
        {
            plFontConfig* ptConfig = &ptFont->_sbtConfigs[j];
            if(ptConfig->bSdf)
            {
                for(uint32_t i = 0u; i < pl_sb_size(ptConfig->_sbtCharData); i++)
                {
                    ptConfig->_sbtCharData[i].x0 = (uint16_t)ptRects[uCharDataOffset + i].x;
                    ptConfig->_sbtCharData[i].y0 = (uint16_t)ptRects[uCharDataOffset + i].y;
                    ptConfig->_sbtCharData[i].x1 = (uint16_t)(ptRects[uCharDataOffset + i].x + ptAtlas->_sbtCustomRects[uCharDataOffset + i].uWidth);
                    ptConfig->_sbtCharData[i].y1 = (uint16_t)(ptRects[uCharDataOffset + i].y + ptAtlas->_sbtCustomRects[uCharDataOffset + i].uHeight);
                }
                uCharDataOffset += pl_sb_size(ptConfig->_sbtCharData);  
            }
        }
        ptFont = ptFont->_ptNextFont;
    }

    // end packing
    stbtt_PackEnd(&tSpc);

    // rasterize SDF/custom rects
    for(uint32_t r = 0; r < uCustomRectCount; r++)
    {
        plFontCustomRect* ptCustomRect = &ptAtlas->_sbtCustomRects[r];
        for(uint32_t i = 0; i < ptCustomRect->uHeight; i++)
        {
            for(uint32_t j = 0; j < ptCustomRect->uWidth; j++)
                ptAtlas->_pucPixelsAsAlpha8[(ptCustomRect->uY + i) * (uint32_t)ptAtlas->tAtlasSize.x + (ptCustomRect->uX + j)] = ptCustomRect->pucBytes[i * ptCustomRect->uWidth + j];
        }
        stbtt_FreeSDF(ptCustomRect->pucBytes, NULL);
        ptCustomRect->pucBytes = NULL;
    }

    // update white point uvs
    ptAtlas->_tWhiteUv.x = (float)(ptAtlas->_ptWhiteRect->uX + ptAtlas->_ptWhiteRect->uWidth / 2) / (float)ptAtlas->tAtlasSize.x;
    ptAtlas->_tWhiteUv.y = (float)(ptAtlas->_ptWhiteRect->uY + ptAtlas->_ptWhiteRect->uHeight / 2) / (float)ptAtlas->tAtlasSize.y;

    // add glyphs
    ptFont = ptAtlas->_ptFontListHead;
    while(ptFont)
    {

        uint32_t uConfigIndex = 0;
        uint32_t uCharIndex = 0;
        float fPixelHeight = 0.0f;
        
        const uint32_t uRangeCount = pl_sb_size(ptFont->_sbtRanges);
        for(uint32_t i = 0; i < uRangeCount; i++)
        {
            plFontRange* ptRange = &ptFont->_sbtRanges[i];
            if(uConfigIndex != ptRange->_uConfigIndex)
            {
                uCharIndex = 0;
                uConfigIndex = ptRange->_uConfigIndex;
            }
            if(ptFont->_sbtConfigs[ptRange->_uConfigIndex].bSdf)
                fPixelHeight = 0.5f * 1.0f / (float)ptAtlas->tAtlasSize.y; // is this correct?
            else
                fPixelHeight = 0.0f;

            for(uint32_t j = 0; j < ptRange->uCharCount; j++)
            {

                const int iCodePoint = ptRange->iFirstCodePoint + j;
                stbtt_aligned_quad tQuad;
                float unused_x = 0.0f;
                float unused_y = 0.0f;
                stbtt_GetPackedQuad((stbtt_packedchar*)ptFont->_sbtConfigs[ptRange->_uConfigIndex]._sbtCharData,
                    (int)ptAtlas->tAtlasSize.x, (int)ptAtlas->tAtlasSize.y, uCharIndex, &unused_x, &unused_y, &tQuad, 0);

                int unusedAdvanced = 0;
                int iLeftSideBearing = 0;
                stbtt_GetCodepointHMetrics(&ptFont->_sbtPreps[ptRange->_uConfigIndex].tFontInfo, iCodePoint,
                    &unusedAdvanced, &iLeftSideBearing);

                plFontGlyph tGlyph = {
                    .x0           = tQuad.x0,
                    .y0           = tQuad.y0 + ptFont->_sbtPreps[ptRange->_uConfigIndex].fAscent,
                    .x1           = tQuad.x1,
                    .y1           = tQuad.y1 + ptFont->_sbtPreps[ptRange->_uConfigIndex].fAscent,
                    .u0           = tQuad.s0,
                    .v0           = tQuad.t0 + fPixelHeight,
                    .u1           = tQuad.s1,
                    .v1           = tQuad.t1 - fPixelHeight,
                    .fXAdvance    = ptFont->_sbtConfigs[ptRange->_uConfigIndex]._sbtCharData[uCharIndex].xAdv,
                    .fLeftBearing = (float)iLeftSideBearing * ptFont->_sbtPreps[ptRange->_uConfigIndex].fScale,
                    .iSDF         = (int)ptFont->_sbtConfigs[ptRange->_uConfigIndex].bSdf
                };
                pl_sb_push(ptFont->_sbtGlyphs, tGlyph);
                ptFont->_auCodePoints[iCodePoint] = pl_sb_size(ptFont->_sbtGlyphs) - 1;
                uCharIndex++;
            }
        }

        for(uint32_t i = 0; i < pl_sb_size(ptFont->_sbtPreps); i++)
        {
            PL_FREE(ptFont->_sbtPreps[i].ptRanges);
            PL_FREE(ptFont->_sbtPreps[i].ptRects);
            PL_FREE(ptFont->_sbtPreps[i].tFontInfo.data);
            ptFont->_sbtPreps[i].tFontInfo.data = NULL;
        }
        pl_sb_free(ptFont->_sbtPreps);

        for(uint32_t i = 0; i < pl_sb_size(ptFont->_sbtConfigs); i++)
        {
            pl_sb_free(ptFont->_sbtConfigs[i]._sbtCharData);
        }
        ptFont = ptFont->_ptNextFont;
    }

    // convert to 4 color channels
    const uint32_t uPixelCount = (uint32_t)(ptAtlas->tAtlasSize.x * ptAtlas->tAtlasSize.y);
    for(uint32_t i = 0; i < uPixelCount; i++)
    {
        ptAtlas->pucPixelsAsRGBA32[i * 4]     = 255;
        ptAtlas->pucPixelsAsRGBA32[i * 4 + 1] = 255;
        ptAtlas->pucPixelsAsRGBA32[i * 4 + 2] = 255;
        ptAtlas->pucPixelsAsRGBA32[i * 4 + 3] = ptAtlas->_pucPixelsAsAlpha8[i];
    }

    PL_FREE(ptRects);
    return true;
}

static void
pl_cleanup_font_atlas(plFontAtlas* ptAtlas)
{
    if(ptAtlas == NULL)
        ptAtlas = gptDrawCtx->ptAtlas;

    plFont* ptFont = ptAtlas->_ptFontListHead;
    while(ptFont)
    {

        PL_FREE(ptFont->_auCodePoints);
        pl_sb_free(ptFont->_sbtGlyphs);
        pl_sb_free(ptFont->_sbtRanges);
        const uint32_t uConfigCount = pl_sb_size(ptFont->_sbtConfigs);
        for(uint32_t j = 0; j < uConfigCount; j++)
        {
            pl_sb_free(ptFont->_sbtConfigs[j]._sbtRanges);
        }
        pl_sb_free(ptFont->_sbtConfigs);
        plFont* ptOldFont = ptFont;
        ptFont = ptFont->_ptNextFont;
        PL_FREE(ptOldFont);
    }

    const uint32_t uRectCount = pl_sb_size(ptAtlas->_sbtCustomRects);
    for(uint32_t i = 0; i < uRectCount; i++)
    {
        PL_FREE(ptAtlas->_sbtCustomRects[i].pucBytes);
    }
    pl_sb_free(ptAtlas->_sbtCustomRects);
    PL_FREE(ptAtlas->_pucPixelsAsAlpha8);
    PL_FREE(ptAtlas->pucPixelsAsRGBA32);
    PL_FREE(ptAtlas);
}

static void
pl_new_draw_3d_frame(void)
{
    // reset 3d drawlists
    for(uint32_t i = 0; i < gptDrawCtx->uDrawlistCount3D; i++)
    {
        plDrawList3D* ptDrawlist = gptDrawCtx->aptDrawlists3D[i];

        pl_sb_reset(ptDrawlist->sbtSolidVertexBuffer);
        pl_sb_reset(ptDrawlist->sbtLineVertexBuffer);
        pl_sb_reset(ptDrawlist->sbtSolidIndexBuffer);    
        pl_sb_reset(ptDrawlist->sbtLineIndexBuffer);    
        pl_sb_reset(ptDrawlist->sbtTextEntries);    
    }

    // reset 3d drawlists
    for(uint32_t i = 0; i < gptDrawCtx->uDrawlistCount2D; i++)
    {
        plDrawList2D* ptDrawlist = gptDrawCtx->aptDrawlists2D[i];

        ptDrawlist->uIndexBufferByteSize = 0;

        pl_sb_reset(ptDrawlist->sbtDrawCommands);
        pl_sb_reset(ptDrawlist->sbtVertexBuffer);
        pl_sb_reset(ptDrawlist->sbuIndexBuffer);
        for(uint32_t j = 0; j < pl_sb_size(ptDrawlist->_sbtLayersCreated); j++)
        {
            pl_sb_reset(ptDrawlist->_sbtLayersCreated[j]->sbtCommandBuffer);
            pl_sb_reset(ptDrawlist->_sbtLayersCreated[j]->sbuIndexBuffer);   
            pl_sb_reset(ptDrawlist->_sbtLayersCreated[j]->sbtPath);  
            ptDrawlist->_sbtLayersCreated[j]->uVertexCount = 0u;
            ptDrawlist->_sbtLayersCreated[j]->ptLastCommand = NULL;
        }
        pl_sb_reset(ptDrawlist->_sbtSubmittedLayers); 
    }
}

static inline void
pl__add_3d_triangles(
        plDrawList3D* ptDrawlist, uint32_t uVertexCount, const plVec3* atPoints,
        uint32_t uTriangleCount, const uint32_t* auIndices, uint32_t uColor)
{

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);
    const uint32_t uIndexStart = pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);

    pl_sb_resize(ptDrawlist->sbtSolidVertexBuffer, pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) + uVertexCount);
    pl_sb_resize(ptDrawlist->sbtSolidIndexBuffer, pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) + 3 * uTriangleCount);

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        ptDrawlist->sbtSolidVertexBuffer[uVertexStart + i] = ((plDrawVertex3DSolid){ {atPoints[i].x, atPoints[i].y, atPoints[i].z}, uColor});
    }

    for(uint32_t i = 0; i < uTriangleCount; i++)
    {
        ptDrawlist->sbtSolidIndexBuffer[uIndexStart + i * 3]     = uVertexStart + auIndices[i * 3];
        ptDrawlist->sbtSolidIndexBuffer[uIndexStart + i * 3 + 1] = uVertexStart + auIndices[i * 3 + 1];
        ptDrawlist->sbtSolidIndexBuffer[uIndexStart + i * 3 + 2] = uVertexStart + auIndices[i * 3 + 2];
    }
}

static void
pl__add_3d_triangle_filled(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plDrawSolidOptions tOptions)
{

    pl_sb_reserve(ptDrawlist->sbtSolidVertexBuffer, pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) + 3);
    pl_sb_reserve(ptDrawlist->sbtSolidIndexBuffer, pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) + 3);

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);

    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP0.x, tP0.y, tP0.z}, tOptions.uColor}));
    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP1.x, tP1.y, tP1.z}, tOptions.uColor}));
    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP2.x, tP2.y, tP2.z}, tOptions.uColor}));

    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 1);
    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 2);
}

static void
pl__add_3d_sphere_filled(plDrawList3D* ptDrawlist, plSphere tDesc, uint32_t uLatBands, uint32_t uLongBands, plDrawSolidOptions tOptions)
{
    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);
    const uint32_t uIndexStart = pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);

    if(uLatBands == 0)
        uLatBands = 16;
    if(uLongBands == 0)
        uLongBands = 16;

    pl_sb_resize(ptDrawlist->sbtSolidVertexBuffer, pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) + (uLatBands + 1) * (uLongBands + 1));
    pl_sb_resize(ptDrawlist->sbtSolidIndexBuffer, pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) + uLatBands * uLongBands * 6);

    uint32_t uCurrentPoint = 0;

    for(uint32_t uLatNumber = 0; uLatNumber <= uLatBands; uLatNumber++)
    {
        const float fTheta = (float)uLatNumber * PL_PI / (float)uLatBands;
        const float fSinTheta = sinf(fTheta);
        const float fCosTheta = cosf(fTheta);
        for(uint32_t uLongNumber = 0; uLongNumber <= uLongBands; uLongNumber++)
        {
            const float fPhi = (float)uLongNumber * 2 * PL_PI / (float)uLongBands;
            const float fSinPhi = sinf(fPhi);
            const float fCosPhi = cosf(fPhi);

            ptDrawlist->sbtSolidVertexBuffer[uVertexStart + uCurrentPoint] = (plDrawVertex3DSolid){ 
                {
                    fCosPhi * fSinTheta * tDesc.fRadius + tDesc.tCenter.x,
                    fCosTheta * tDesc.fRadius + tDesc.tCenter.y,
                    fSinPhi * fSinTheta * tDesc.fRadius + tDesc.tCenter.z}, 
                tOptions.uColor};
            uCurrentPoint++;
        }
    }

    uCurrentPoint = 0;
    for(uint32_t uLatNumber = 0; uLatNumber < uLatBands; uLatNumber++)
    {

        for(uint32_t uLongNumber = 0; uLongNumber < uLongBands; uLongNumber++)
        {
			const uint32_t uFirst = (uLatNumber * (uLongBands + 1)) + uLongNumber;
			const uint32_t uSecond = uFirst + uLongBands + 1;

            ptDrawlist->sbtSolidIndexBuffer[uIndexStart + uCurrentPoint + 0] = uVertexStart + uFirst;
            ptDrawlist->sbtSolidIndexBuffer[uIndexStart + uCurrentPoint + 1] = uVertexStart + uSecond;
            ptDrawlist->sbtSolidIndexBuffer[uIndexStart + uCurrentPoint + 2] = uVertexStart + uFirst + 1;

            ptDrawlist->sbtSolidIndexBuffer[uIndexStart + uCurrentPoint + 3] = uVertexStart + uSecond;
            ptDrawlist->sbtSolidIndexBuffer[uIndexStart + uCurrentPoint + 4] = uVertexStart + uSecond + 1;
            ptDrawlist->sbtSolidIndexBuffer[uIndexStart + uCurrentPoint + 5] = uVertexStart + uFirst + 1;

            uCurrentPoint += 6;
        }
    }
}

static void
pl__add_3d_circle_xz_filled(plDrawList3D* ptDrawlist, plVec3 tCenter, float fRadius, uint32_t uSegments, plDrawSolidOptions tOptions)
{
    if(uSegments == 0){ uSegments = 12; }
    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * (uSegments + 2));
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * (uSegments * 3 + 3));
    atPoints[0] = tCenter;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i + 1] = (plVec3){tCenter.x + fRadius * sinf(fTheta + PL_PI_2), tCenter.y, tCenter.z + fRadius * sinf(fTheta)};
        fTheta += fIncrement;
    }
    atPoints[uSegments + 1] = atPoints[1];

    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[i * 3] = 0;
        auIndices[i * 3 + 1] = i + 1;
        auIndices[i * 3 + 2] = i;
    }
    auIndices[uSegments * 3] = 0;
    auIndices[uSegments * 3 + 1] = 1;
    auIndices[uSegments * 3 + 2] = uSegments;

    pl__add_3d_triangles(ptDrawlist, uSegments + 2, atPoints, uSegments + 1, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_centered_box_filled(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plDrawSolidOptions tOptions)
{

    const float fHalfWidth = fWidth / 2.0f;
    const float fHalfHeight = fHeight / 2.0f;
    const float fHalfDepth = fDepth / 2.0f;

    const plVec3 tWidthVec  = {fHalfWidth, 0.0f, 0.0f};
    const plVec3 tHeightVec = {0.0f, fHalfHeight, 0.0f};
    const plVec3 tDepthVec  = {0.0f, 0.0f, fHalfDepth};

    const plVec3 atVerticies[8] = {
        {  tCenter.x - fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x - fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x - fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z + fHalfDepth},
        {  tCenter.x - fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z + fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z + fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z + fHalfDepth}
    };

    const uint32_t auIndices[] = {
        0, 3, 2,
        0, 2, 1,
        3, 7, 2,
        7, 6, 2,
        7, 4, 6,
        4, 5, 6,
        4, 0, 5,
        0, 1, 5,
        4, 7, 3,
        0, 4, 3,
        5, 1, 2,
        5, 2, 6
    };

    pl__add_3d_triangles(ptDrawlist, 8, atVerticies, 12, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_plane_xz_filled(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, plDrawSolidOptions tOptions)
{

    const float fHalfWidth = fWidth / 2.0f;
    const float fHalfHeight = fHeight / 2.0f;

    const plVec3 tWidthVec  = {fHalfWidth, 0.0f, 0.0f};
    const plVec3 tHeightVec = {0.0f, fHalfHeight, 0.0f};

    const plVec3 atVerticies[] = {
        {  tCenter.x - fHalfWidth,  tCenter.y, tCenter.z - fHalfHeight},
        {  tCenter.x - fHalfWidth,  tCenter.y, tCenter.z + fHalfHeight},
        {  tCenter.x + fHalfWidth,  tCenter.y, tCenter.z + fHalfHeight},
        {  tCenter.x + fHalfWidth,  tCenter.y, tCenter.z - fHalfHeight}
    };

    const uint32_t auIndices[] = {
        0, 1, 2,
        0, 2, 3
    };

    pl__add_3d_triangles(ptDrawlist, 4, atVerticies, 2, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_plane_xy_filled(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, plDrawSolidOptions tOptions)
{

    const float fHalfWidth = fWidth / 2.0f;
    const float fHalfHeight = fHeight / 2.0f;

    const plVec3 tWidthVec  = {fHalfWidth, 0.0f, 0.0f};
    const plVec3 tHeightVec = {0.0f, fHalfHeight, 0.0f};

    const plVec3 atVerticies[] = {
        {  tCenter.x - fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z},
        {  tCenter.x - fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z},
        {  tCenter.x + fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z},
        {  tCenter.x + fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z}
    };

    const uint32_t auIndices[] = {
        0, 1, 2,
        0, 2, 3
    };

    pl__add_3d_triangles(ptDrawlist, 4, atVerticies, 2, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_plane_yz_filled(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, plDrawSolidOptions tOptions)
{
    const float fHalfWidth = fWidth / 2.0f;
    const float fHalfHeight = fHeight / 2.0f;
    const plVec3 atVerticies[] = {
        {  tCenter.x, tCenter.y - fHalfHeight,  tCenter.z - fHalfWidth},
        {  tCenter.x, tCenter.y - fHalfHeight,  tCenter.z + fHalfWidth},
        {  tCenter.x, tCenter.y + fHalfHeight,  tCenter.z + fHalfWidth},
        {  tCenter.x, tCenter.y + fHalfHeight,  tCenter.z - fHalfWidth}
    };

    const uint32_t auIndices[] = {
        0, 1, 2,
        0, 2, 3
    };

    pl__add_3d_triangles(ptDrawlist, 4, atVerticies, 2, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_band_xz_filled(plDrawList3D* ptDrawlist, plVec3 tCenter, float fInnerRadius, float fOuterRadius, uint32_t uSegments, plDrawSolidOptions tOptions)
{
    if(uSegments == 0)
        uSegments = 12;

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uSegments * 2);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uSegments * 2 * 3);
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){
            tCenter.x + fOuterRadius * sinf(fTheta + PL_PI_2),
            tCenter.y, tCenter.z + fOuterRadius * sinf(fTheta)
        };
        atPoints[i + uSegments] = (plVec3){
            tCenter.x + fInnerRadius * sinf(fTheta + PL_PI_2),
            tCenter.y, tCenter.z + fInnerRadius * sinf(fTheta)
        };
        fTheta += fIncrement;
    }

    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[i * 6] = i;
        auIndices[i * 6 + 1] = i + uSegments;
        auIndices[i * 6 + 2] = i + uSegments + 1;

        auIndices[i * 6 + 3] = i;
        auIndices[i * 6 + 4] = i + uSegments + 1;
        auIndices[i * 6 + 5] = i + 1;
    }
    auIndices[(uSegments - 1) * 6 + 2] = uSegments;
    auIndices[(uSegments - 1) * 6 + 4] = uSegments;
    auIndices[(uSegments - 1) * 6 + 5] = 0;

    pl__add_3d_triangles(ptDrawlist, uSegments * 2, atPoints, uSegments * 2, auIndices, tOptions.uColor);

    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_band_xy_filled(
    plDrawList3D* ptDrawlist, plVec3 tCenter, float fInnerRadius,
    float fOuterRadius, uint32_t uSegments, plDrawSolidOptions tOptions)
{
    if(uSegments == 0)
        uSegments = 12;

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uSegments * 2);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uSegments * 2 * 3);
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){tCenter.x + fOuterRadius * sinf(fTheta + PL_PI_2), tCenter.y + fOuterRadius * sinf(fTheta), tCenter.z};
        atPoints[i + uSegments] = (plVec3){tCenter.x + fInnerRadius * sinf(fTheta + PL_PI_2), tCenter.y + fInnerRadius * sinf(fTheta), tCenter.z};
        fTheta += fIncrement;
    }

    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[i * 6] = i;
        auIndices[i * 6 + 1] = i + uSegments;
        auIndices[i * 6 + 2] = i + uSegments + 1;

        auIndices[i * 6 + 3] = i;
        auIndices[i * 6 + 4] = i + uSegments + 1;
        auIndices[i * 6 + 5] = i + 1;
    }
    auIndices[(uSegments - 1) * 6 + 2] = uSegments;
    auIndices[(uSegments - 1) * 6 + 4] = uSegments;
    auIndices[(uSegments - 1) * 6 + 5] = 0;

    pl__add_3d_triangles(ptDrawlist, uSegments * 2, atPoints, uSegments * 2, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_band_yz_filled(
    plDrawList3D* ptDrawlist, plVec3 tCenter, float fInnerRadius,
    float fOuterRadius, uint32_t uSegments, plDrawSolidOptions tOptions)
{
    if(uSegments == 0)
        uSegments = 12;

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uSegments * 2);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uSegments * 2 * 3);
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){
            tCenter.x, tCenter.y + fOuterRadius * sinf(fTheta + PL_PI_2),
            tCenter.z + fOuterRadius * sinf(fTheta)
        };
        atPoints[i + uSegments] = (plVec3){
            tCenter.x, tCenter.y + fInnerRadius * sinf(fTheta + PL_PI_2),
            tCenter.z + fInnerRadius * sinf(fTheta)
        };
        fTheta += fIncrement;
    }

    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[i * 6] = i;
        auIndices[i * 6 + 1] = i + uSegments;
        auIndices[i * 6 + 2] = i + uSegments + 1;

        auIndices[i * 6 + 3] = i;
        auIndices[i * 6 + 4] = i + uSegments + 1;
        auIndices[i * 6 + 5] = i + 1;
    }
    auIndices[(uSegments - 1) * 6 + 2] = uSegments;
    auIndices[(uSegments - 1) * 6 + 4] = uSegments;
    auIndices[(uSegments - 1) * 6 + 5] = 0;

    pl__add_3d_triangles(ptDrawlist, uSegments * 2, atPoints, uSegments * 2, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_cylinder_filled(plDrawList3D* ptDrawlist, plCylinder tDesc, uint32_t uSegments, plDrawSolidOptions tOptions)
{

    if(uSegments == 0)
        uSegments = 12;

    plVec3 tDirection = pl_sub_vec3(tDesc.tTipPos, tDesc.tBasePos);
    const float fDistance = pl_length_vec3(tDirection);
    tDirection = pl_norm_vec3(tDirection);
    const float fAngleBetweenVecs = acosf(pl_dot_vec3(tDirection, (plVec3){0.0f, 1.0f, 0.0f}));
    const plVec3 tRotAxis = pl_cross_vec3((plVec3){0.0f, 1.0f, 0.0f}, tDirection);
    const plMat4 tRot = pl_mat4_rotate_vec3(fAngleBetweenVecs, tRotAxis);
    
    const uint32_t uPointCount = uSegments * 2 + 2;
    const uint32_t uIndexCount = (uSegments * 2 * 3) + (2 * 3 * uSegments);
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uPointCount);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uIndexCount);

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){tDesc.fRadius * sinf(fTheta + PL_PI_2), 0.0f, tDesc.fRadius * sinf(fTheta)};
        atPoints[i + uSegments] = (plVec3){atPoints[i].x, atPoints[i].y + fDistance, atPoints[i].z};
        atPoints[i] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[i]}).xyz;
        atPoints[i + uSegments] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[i + uSegments]}).xyz;
        atPoints[i] = pl_add_vec3(atPoints[i], tDesc.tBasePos);
        atPoints[i + uSegments] = pl_add_vec3(atPoints[i + uSegments], tDesc.tBasePos);
        fTheta += fIncrement;
    }
    atPoints[uPointCount - 2] = tDesc.tBasePos;
    atPoints[uPointCount - 1] = tDesc.tTipPos;

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[i * 6] = i;
        auIndices[i * 6 + 1] = i + uSegments;
        auIndices[i * 6 + 2] = i + uSegments + 1;

        auIndices[i * 6 + 3] = i;
        auIndices[i * 6 + 4] = i + uSegments + 1;
        auIndices[i * 6 + 5] = i + 1;
        uCurrentIndex += 6;
    }
    auIndices[(uSegments - 1) * 6 + 2] = uSegments;
    auIndices[(uSegments - 1) * 6 + 4] = uSegments;
    auIndices[(uSegments - 1) * 6 + 5] = 0;

    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[uCurrentIndex + i * 6] = uPointCount - 2;
        auIndices[uCurrentIndex + i * 6 + 1] = i + 1;
        auIndices[uCurrentIndex + i * 6 + 2] = i;

        auIndices[uCurrentIndex + i * 6 + 3] = uPointCount - 1;
        auIndices[uCurrentIndex + i * 6 + 4] = i + 1 + uSegments;
        auIndices[uCurrentIndex + i * 6 + 5] = i + uSegments;
    }
    auIndices[uCurrentIndex + (uSegments - 1) * 6 + 1] = 0;
    auIndices[uCurrentIndex + (uSegments - 1) * 6 + 4] = uSegments;

    pl__add_3d_triangles(ptDrawlist, uPointCount, atPoints, uIndexCount / 3, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_cone_filled(plDrawList3D* ptDrawlist, plCone tDesc, uint32_t uSegments, plDrawSolidOptions tOptions)
{

    if(uSegments == 0)
        uSegments = 12;

    plVec3 tDirection = pl_sub_vec3(tDesc.tTipPos, tDesc.tBasePos);
    const float fDistance = pl_length_vec3(tDirection);
    tDirection = pl_norm_vec3(tDirection);
    const float fAngleBetweenVecs = acosf(pl_dot_vec3(tDirection, (plVec3){0.0f, 1.0f, 0.0f}));
    const plVec3 tRotAxis = pl_cross_vec3((plVec3){0.0f, 1.0f, 0.0f}, tDirection);
    const plMat4 tRot = pl_mat4_rotate_vec3(fAngleBetweenVecs, tRotAxis);
    
    const uint32_t uPointCount = uSegments + 2;
    const uint32_t uIndexCount = uSegments * 2 * 3;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uPointCount);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uIndexCount);

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){tDesc.fRadius * sinf(fTheta + PL_PI_2), 0.0f, tDesc.fRadius * sinf(fTheta)};
        atPoints[i] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[i]}).xyz;
        atPoints[i] = pl_add_vec3(atPoints[i], tDesc.tBasePos);
        fTheta += fIncrement;
    }
    atPoints[uPointCount - 2] = tDesc.tBasePos;
    atPoints[uPointCount - 1] = tDesc.tTipPos;

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[i * 6]     = i;
        auIndices[i * 6 + 1] = i + 1;
        auIndices[i * 6 + 2] = uPointCount - 2;

        auIndices[i * 6 + 3] = i;
        auIndices[i * 6 + 4] = uPointCount - 1;
        auIndices[i * 6 + 5] = i + 1;

        uCurrentIndex+=6;
    }
    uCurrentIndex-=6;
    auIndices[uCurrentIndex + 1] = 0;
    auIndices[uCurrentIndex + 5] = 0;

    pl__add_3d_triangles(ptDrawlist, uPointCount, atPoints, uIndexCount / 3, auIndices, tOptions.uColor);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_line(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plDrawLineOptions tOptions)
{
    pl_sb_reserve(ptDrawlist->sbtLineVertexBuffer, pl_sb_size(ptDrawlist->sbtLineVertexBuffer) + 4);
    pl_sb_reserve(ptDrawlist->sbtLineIndexBuffer, pl_sb_size(ptDrawlist->sbtLineIndexBuffer) + 6);

    plDrawVertex3DLine tNewVertex0 = {
        {tP0.x, tP0.y, tP0.z},
        -1.0f,
        tOptions.fThickness,
        1.0f,
        {tP1.x, tP1.y, tP1.z},
        tOptions.uColor
    };

    plDrawVertex3DLine tNewVertex1 = {
        {tP1.x, tP1.y, tP1.z},
        -1.0f,
        tOptions.fThickness,
        -1.0f,
        {tP0.x, tP0.y, tP0.z},
        tOptions.uColor
    };

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtLineVertexBuffer);
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex0);
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex1);

    tNewVertex0.fDirection = 1.0f;
    tNewVertex1.fDirection = 1.0f;
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex1);
    pl_sb_push(ptDrawlist->sbtLineVertexBuffer, tNewVertex0);

    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 1);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 2);

    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 2);
    pl_sb_push(ptDrawlist->sbtLineIndexBuffer, uVertexStart + 3);
}

static void
pl__add_3d_text(plDrawList3D* ptDrawlist, plVec3 tP, const char* pcText, plDrawTextOptions tOptions)
{
    plDraw3DText tText = {
        .fSize       = tOptions.fSize == 0.0f ? tOptions.ptFont->fSize : tOptions.fSize,
        .fWrap       = tOptions.fWrap,
        .uColor      = tOptions.uColor,
        .ptFont      = tOptions.ptFont,
        .tP          = tP
    };
    strncpy(tText.acText, pcText, PL_MAX_NAME_LENGTH);
    pl_sb_push(ptDrawlist->sbtTextEntries, tText);
}

static void
pl__add_3d_cross(plDrawList3D* ptDrawlist, plVec3 tP, float fLength, plDrawLineOptions tOptions)
{
    const float fHalfLength = fLength / 2.0f;
    const plVec3 aatVerticies[6] = {
        {  tP.x - fHalfLength,  tP.y, tP.z},
        {  tP.x + fHalfLength,  tP.y, tP.z},
        {  tP.x,  tP.y - fHalfLength, tP.z},
        {  tP.x,  tP.y + fHalfLength, tP.z},
        {  tP.x,  tP.y, tP.z - fHalfLength},
        {  tP.x,  tP.y, tP.z + fHalfLength}
    };
    pl__add_3d_lines(ptDrawlist, 3, aatVerticies, tOptions);
}

static void
pl__add_3d_transform(plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, plDrawLineOptions tOptions)
{

    const plVec3 tOrigin = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, 0.0f});
    const plVec3 tXAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){fLength, 0.0f, 0.0f});
    const plVec3 tYAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, fLength, 0.0f});
    const plVec3 tZAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, fLength});

    tOptions.uColor = PL_COLOR_32_RGB(1.0f, 0.0f, 0.0f);
    pl__add_3d_line(ptDrawlist, tOrigin, tXAxis, tOptions);
    tOptions.uColor = PL_COLOR_32_RGB(0.0f, 1.0f, 0.0f);
    pl__add_3d_line(ptDrawlist, tOrigin, tYAxis, tOptions);
    tOptions.uColor = PL_COLOR_32_RGB(0.0f, 0.0f, 1.0f);
    pl__add_3d_line(ptDrawlist, tOrigin, tZAxis, tOptions);
}

static void
pl__add_3d_frustum(plDrawList3D* ptDrawlist, const plMat4* ptTransform, plDrawFrustumDesc tDesc, plDrawLineOptions tOptions)
{
    const float fSmallHeight = tanf(tDesc.fYFov / 2.0f) * tDesc.fNearZ;
    const float fSmallWidth  = fSmallHeight * tDesc.fAspectRatio;
    const float fBigHeight   = tanf(tDesc.fYFov / 2.0f) * tDesc.fFarZ;
    const float fBigWidth    = fBigHeight * tDesc.fAspectRatio;

    const plVec3 atVerticies[8] = {
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth,  fSmallHeight, tDesc.fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth, -fSmallHeight, tDesc.fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth, -fSmallHeight, tDesc.fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth,  fSmallHeight, tDesc.fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,    fBigHeight,   tDesc.fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,   -fBigHeight,   tDesc.fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,   -fBigHeight,   tDesc.fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,    fBigHeight,   tDesc.fFarZ})
    };

    const uint32_t auIndices[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        0, 4,
        1, 5,
        2, 6,
        3, 7,
        4, 5,
        5, 6,
        6, 7,
        7, 4
    };
    pl__add_3d_indexed_lines(ptDrawlist, 24, atVerticies, auIndices, tOptions);
}

static void
pl__add_3d_sphere_ex(plDrawList3D* ptDrawlist, plSphere tSphere, uint32_t uLatBands, uint32_t uLongBands, plDrawLineOptions tOptions)
{
    if(uLatBands == 0)
        uLatBands = 16;
    if(uLongBands == 0)
        uLongBands = 16;
    
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator,
        sizeof(plVec3) * (uLatBands + 1) * (uLongBands + 1));
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator,
        sizeof(uint32_t) * uLatBands * uLongBands * 8);
    uint32_t uCurrentPoint = 0;

    for(uint32_t uLatNumber = 0; uLatNumber <= uLatBands; uLatNumber++)
    {
        const float fTheta = (float)uLatNumber * PL_PI / (float)uLatBands;
        const float fSinTheta = sinf(fTheta);
        const float fCosTheta = cosf(fTheta);
        for(uint32_t uLongNumber = 0; uLongNumber <= uLongBands; uLongNumber++)
        {
            const float fPhi = (float)uLongNumber * 2 * PL_PI / (float)uLongBands;
            const float fSinPhi = sinf(fPhi);
            const float fCosPhi = cosf(fPhi);
            atPoints[uCurrentPoint] = (plVec3){
                fCosPhi * fSinTheta * tSphere.fRadius + tSphere.tCenter.x,
                fCosTheta * tSphere.fRadius + tSphere.tCenter.y,
                fSinPhi * fSinTheta * tSphere.fRadius + tSphere.tCenter.z
            };
            uCurrentPoint++;
        }
    }

    uCurrentPoint = 0;
    for(uint32_t uLatNumber = 0; uLatNumber < uLatBands; uLatNumber++)
    {

        for(uint32_t uLongNumber = 0; uLongNumber < uLongBands; uLongNumber++)
        {
			const uint32_t uFirst = (uLatNumber * (uLongBands + 1)) + uLongNumber;
			const uint32_t uSecond = uFirst + uLongBands + 1;
            auIndices[uCurrentPoint] = uFirst;
            auIndices[uCurrentPoint + 1] = uSecond;

            auIndices[uCurrentPoint + 2] = uSecond;
            auIndices[uCurrentPoint + 3] = uSecond + 1;

            auIndices[uCurrentPoint + 4] = uSecond + 1;
            auIndices[uCurrentPoint + 5] = uFirst + 1;

            auIndices[uCurrentPoint + 6] = uFirst;
            auIndices[uCurrentPoint + 7] = uFirst + 1;

            uCurrentPoint += 8;
        }
    }
    pl__add_3d_indexed_lines(ptDrawlist, uLatBands * uLongBands * 8, atPoints, auIndices, tOptions);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_capsule_ex(plDrawList3D* ptDrawlist, plCapsule tDesc, uint32_t uLatBands, uint32_t uLongBands, plDrawLineOptions tOptions)
{
    if(uLatBands == 0)
        uLatBands = 16;
    if(uLongBands == 0)
        uLongBands = 16;

    float fTipRadius = tDesc.fRadius;
    float fBaseRadius = tDesc.fRadius;
    float fEndOffsetRatio = 0.0f;

    fTipRadius = fTipRadius < 0.0f ? fBaseRadius : fTipRadius;

    plVec3 tDirection = pl_sub_vec3(tDesc.tTipPos, tDesc.tBasePos);
    const float fDistance = pl_length_vec3(tDirection);
    tDirection = pl_norm_vec3(tDirection);
    const float fAngleBetweenVecs = acosf(pl_dot_vec3(tDirection, (plVec3){0.0f, 1.0f, 0.0f}));
    const plVec3 tRotAxis = pl_cross_vec3((plVec3){0.0f, 1.0f, 0.0f}, tDirection);
    const plMat4 tRot = pl_mat4_rotate_vec3(fAngleBetweenVecs, tRotAxis);
    
    const uint32_t uPointCount = (uLatBands + 1) * (uLongBands + 1);
    const uint32_t uIndexCount = uLatBands * uLongBands * 8;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uPointCount);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uIndexCount);
    uint32_t uCurrentPoint = 0;

    for(uint32_t uLatNumber = 0; uLatNumber <= uLatBands / 2; uLatNumber++)
    {
        const float fTheta = (float)uLatNumber * PL_PI_2 / ((float)uLatBands / 2.0f);
        const float fSinTheta = sinf(fTheta);
        const float fCosTheta = cosf(fTheta);
        for(uint32_t uLongNumber = 0; uLongNumber <= uLongBands; uLongNumber++)
        {
            const float fPhi = (float)uLongNumber * 2 * PL_PI / (float)uLongBands;
            const float fSinPhi = sinf(fPhi);
            const float fCosPhi = cosf(fPhi);
            atPoints[uCurrentPoint] = (plVec3){
                fCosPhi * fSinTheta * fTipRadius,
                fCosTheta * fTipRadius + fDistance - fTipRadius * (1.0f - fEndOffsetRatio),
                fSinPhi * fSinTheta * fTipRadius
            };
            atPoints[uCurrentPoint] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[uCurrentPoint]}).xyz;
            atPoints[uCurrentPoint].x += tDesc.tBasePos.x;
            atPoints[uCurrentPoint].y += tDesc.tBasePos.y;
            atPoints[uCurrentPoint].z += tDesc.tBasePos.z;
            uCurrentPoint++;
        }
    }

    for(uint32_t uLatNumber = 1; uLatNumber <= uLatBands / 2; uLatNumber++)
    {
        const float fTheta = PL_PI_2 + (float)uLatNumber * PL_PI_2 / ((float)uLatBands / 2.0f);
        const float fSinTheta = sinf(fTheta);
        const float fCosTheta = cosf(fTheta);
        for(uint32_t uLongNumber = 0; uLongNumber <= uLongBands; uLongNumber++)
        {
            const float fPhi = (float)uLongNumber * 2 * PL_PI / (float)uLongBands;
            const float fSinPhi = sinf(fPhi);
            const float fCosPhi = cosf(fPhi);
            atPoints[uCurrentPoint] = (plVec3){
                fCosPhi * fSinTheta * fBaseRadius,
                fCosTheta * fBaseRadius + fBaseRadius * (1.0f - fEndOffsetRatio),
                fSinPhi * fSinTheta * fBaseRadius
            };
            atPoints[uCurrentPoint] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[uCurrentPoint]}).xyz;
            atPoints[uCurrentPoint].x += tDesc.tBasePos.x;
            atPoints[uCurrentPoint].y += tDesc.tBasePos.y;
            atPoints[uCurrentPoint].z += tDesc.tBasePos.z;
            uCurrentPoint++;
        }
    }

    uCurrentPoint = 0;
    for(uint32_t uLatNumber = 0; uLatNumber < uLatBands; uLatNumber++)
    {

        for(uint32_t uLongNumber = 0; uLongNumber < uLongBands; uLongNumber++)
        {
			const uint32_t uFirst = (uLatNumber * (uLongBands + 1)) + uLongNumber;
			const uint32_t uSecond = uFirst + uLongBands + 1;
            auIndices[uCurrentPoint] = uFirst;
            auIndices[uCurrentPoint + 1] = uSecond;

            auIndices[uCurrentPoint + 2] = uSecond;
            auIndices[uCurrentPoint + 3] = uSecond + 1;

            auIndices[uCurrentPoint + 4] = uSecond + 1;
            auIndices[uCurrentPoint + 5] = uFirst + 1;

            auIndices[uCurrentPoint + 6] = uFirst;
            auIndices[uCurrentPoint + 7] = uFirst + 1;

            uCurrentPoint += 8;
        }
    }
    pl__add_3d_indexed_lines(ptDrawlist, uIndexCount, atPoints, auIndices, tOptions);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_cylinder(plDrawList3D* ptDrawlist, plCylinder tDesc, uint32_t uSegments, plDrawLineOptions tOptions)
{

    if(uSegments == 0)
        uSegments = 12;

    plVec3 tDirection = pl_sub_vec3(tDesc.tTipPos, tDesc.tBasePos);
    const float fDistance = pl_length_vec3(tDirection);
    tDirection = pl_norm_vec3(tDirection);
    const float fAngleBetweenVecs = acosf(pl_dot_vec3(tDirection, (plVec3){0.0f, 1.0f, 0.0f}));
    const plVec3 tRotAxis = pl_cross_vec3((plVec3){0.0f, 1.0f, 0.0f}, tDirection);
    const plMat4 tRot = pl_mat4_rotate_vec3(fAngleBetweenVecs, tRotAxis);
    
    const uint32_t uPointCount = uSegments * 2;
    const uint32_t uIndexCount = uSegments * 8 - 2;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uPointCount);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uIndexCount);

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){tDesc.fRadius * sinf(fTheta + PL_PI_2), 0.0f, tDesc.fRadius * sinf(fTheta)};
        atPoints[i + uSegments] = (plVec3){atPoints[i].x, atPoints[i].y + fDistance, atPoints[i].z};
        atPoints[i] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[i]}).xyz;
        atPoints[i + uSegments] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[i + uSegments]}).xyz;
        atPoints[i] = pl_add_vec3(atPoints[i], tDesc.tBasePos);
        atPoints[i + uSegments] = pl_add_vec3(atPoints[i + uSegments], tDesc.tBasePos);
        fTheta += fIncrement;
    }

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[uCurrentIndex] = i;
        auIndices[uCurrentIndex + 1] = i + 1;
        auIndices[uCurrentIndex + 2] = i + uSegments;
        auIndices[uCurrentIndex + 3] = i + uSegments + 1;

        auIndices[uCurrentIndex + 4] = i;
        auIndices[uCurrentIndex + 5] = i + uSegments;

        auIndices[uCurrentIndex + 6] = i + 1;
        auIndices[uCurrentIndex + 7] = i + 1 + uSegments;
        uCurrentIndex += 8;
    }
    uCurrentIndex -= 8;
    auIndices[uCurrentIndex + 1] = 0;
    auIndices[uCurrentIndex + 3] = uSegments;

    pl__add_3d_indexed_lines(ptDrawlist, uIndexCount, atPoints, auIndices, tOptions);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_cone_ex(plDrawList3D* ptDrawlist, plCone tDesc, uint32_t uSegments, plDrawLineOptions tOptions)
{

    if(uSegments == 0)
        uSegments = 12;

    plVec3 tDirection = pl_sub_vec3(tDesc.tTipPos, tDesc.tBasePos);
    const float fDistance = pl_length_vec3(tDirection);
    tDirection = pl_norm_vec3(tDirection);
    const float fAngleBetweenVecs = acosf(pl_dot_vec3(tDirection, (plVec3){0.0f, 1.0f, 0.0f}));
    const plVec3 tRotAxis = pl_cross_vec3((plVec3){0.0f, 1.0f, 0.0f}, tDirection);
    const plMat4 tRot = pl_mat4_rotate_vec3(fAngleBetweenVecs, tRotAxis);
    
    const uint32_t uPointCount = uSegments + 1;
    const uint32_t uIndexCount = uSegments * 2 * 2;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * uPointCount);
    uint32_t* auIndices = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(uint32_t) * uIndexCount);

    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){tDesc.fRadius * sinf(fTheta + PL_PI_2), 0.0f, tDesc.fRadius * sinf(fTheta)};
        atPoints[i] = pl_mul_mat4_vec4(&tRot, (plVec4){.xyz = atPoints[i]}).xyz;
        atPoints[i] = pl_add_vec3(atPoints[i], tDesc.tBasePos);
        fTheta += fIncrement;
    }
    atPoints[uPointCount - 1] = tDesc.tTipPos;

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[i * 2]     = i;
        auIndices[i * 2 + 1] = i + 1;
        uCurrentIndex+=2;
    }
    uCurrentIndex-=2;
    auIndices[uCurrentIndex + 1] = 0;
    uCurrentIndex+=2;

    for(uint32_t i = 0; i < uSegments; i++)
    {
        auIndices[uCurrentIndex + i * 2]     = i;
        auIndices[uCurrentIndex + i * 2 + 1] = uPointCount - 1;
    }

    pl__add_3d_indexed_lines(ptDrawlist, uIndexCount, atPoints, auIndices, tOptions);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_circle_xz(plDrawList3D* ptDrawlist, plVec3 tCenter, float fRadius, uint32_t uSegments, plDrawLineOptions tOptions)
{
    if(uSegments == 0){ uSegments = 12; }
    const float fIncrement = PL_2PI / uSegments;
    float fTheta = 0.0f;
    plVec3* atPoints = pl_temp_allocator_alloc(&gptDrawCtx->tTempAllocator, sizeof(plVec3) * (uSegments + 1));
    for(uint32_t i = 0; i < uSegments; i++)
    {
        atPoints[i] = (plVec3){tCenter.x + fRadius * sinf(fTheta + PL_PI_2), tCenter.y, tCenter.z + fRadius * sinf(fTheta)};
        fTheta += fIncrement;
    }
    atPoints[uSegments] = atPoints[0];
    pl__add_3d_path(ptDrawlist, uSegments + 1, atPoints, tOptions);
    pl_temp_allocator_reset(&gptDrawCtx->tTempAllocator);
}

static void
pl__add_3d_centered_box(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plDrawLineOptions tOptions)
{

    const float fHalfWidth = fWidth / 2.0f;
    const float fHalfHeight = fHeight / 2.0f;
    const float fHalfDepth = fDepth / 2.0f;

    const plVec3 tWidthVec  = {fHalfWidth, 0.0f, 0.0f};
    const plVec3 tHeightVec = {0.0f, fHalfHeight, 0.0f};
    const plVec3 tDepthVec  = {0.0f, 0.0f, fHalfDepth};

    const plVec3 atVerticies[8] = {
        {  tCenter.x - fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x - fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z - fHalfDepth},
        {  tCenter.x - fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z + fHalfDepth},
        {  tCenter.x - fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z + fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y - fHalfHeight, tCenter.z + fHalfDepth},
        {  tCenter.x + fHalfWidth,  tCenter.y + fHalfHeight, tCenter.z + fHalfDepth}
    };

    const uint32_t auIndices[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        0, 4,
        1, 5,
        2, 6,
        3, 7,
        4, 5,
        5, 6,
        6, 7,
        7, 4
    };
    pl__add_3d_indexed_lines(ptDrawlist, 24, atVerticies, auIndices, tOptions);
}

static void
pl__add_3d_aabb(plDrawList3D* ptDrawlist, plVec3 tMin, plVec3 tMax, plDrawLineOptions tOptions)
{
    const plVec3 atVerticies[] = {
        {  tMin.x, tMin.y, tMin.z },
        {  tMax.x, tMin.y, tMin.z },
        {  tMax.x, tMax.y, tMin.z },
        {  tMin.x, tMax.y, tMin.z },
        {  tMin.x, tMin.y, tMax.z },
        {  tMax.x, tMin.y, tMax.z },
        {  tMax.x, tMax.y, tMax.z },
        {  tMin.x, tMax.y, tMax.z },
    };

    const uint32_t auIndices[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        0, 4,
        1, 5,
        2, 6,
        3, 7,
        4, 5,
        5, 6,
        6, 7,
        7, 4
    };
    pl__add_3d_indexed_lines(ptDrawlist, 24, atVerticies, auIndices, tOptions);
}

static void
pl__add_3d_bezier_quad(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, uint32_t uSegments, plDrawLineOptions tOptions)
{

    // order of the bezier curve inputs are 0=start, 1=control, 2=ending

    if(uSegments == 0)
        uSegments = 12;

    // set up first point
    plVec3 atVerticies[2] = {(plVec3){0.0, 0.0, 0.0},tP0};

    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        
        const plVec3 p0 = pl_mul_vec3_scalarf(tP0, uu);
        const plVec3 p1 = pl_mul_vec3_scalarf(tP1, (2.0f * u * t)); 
        const plVec3 p2 = pl_mul_vec3_scalarf(tP2, tt); 
        const plVec3 p3 = pl_add_vec3(p0,p1);
        const plVec3 p4 = pl_add_vec3(p2,p3);
        
        // shift and add next point
        atVerticies[0] = atVerticies[1];
        atVerticies[1] = p4;

        pl__add_3d_line(ptDrawlist, atVerticies[0], atVerticies[1], tOptions);
    }

    // set up last point
    atVerticies[0] = atVerticies[1];
    atVerticies[1] = tP2;
    pl__add_3d_line(ptDrawlist, atVerticies[0], atVerticies[1], tOptions);
}

static void
pl__add_3d_bezier_cubic(
    plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2,
    plVec3 tP3, uint32_t uSegments, plDrawLineOptions tOptions)
{
    // order of the bezier curve inputs are 0=start, 1=control 1, 2=control 2, 3=ending

    if(uSegments == 0)
        uSegments = 12;

    // set up first point
    plVec3 atVerticies[2] = {(plVec3){0.0, 0.0, 0.0},tP0};

    for (int i = 1; i < (int)uSegments; i++)
    {
        const float t = i / (float)uSegments;
        const float u = 1.0f - t;
        const float tt = t * t;
        const float uu = u * u;
        const float uuu = uu * u;
        const float ttt = tt * t;
        
        const plVec3 p0 = pl_mul_vec3_scalarf(tP0, uuu);
        const plVec3 p1 = pl_mul_vec3_scalarf(tP1, (3.0f * uu * t)); 
        const plVec3 p2 = pl_mul_vec3_scalarf(tP2, (3.0f * u * tt)); 
        const plVec3 p3 = pl_mul_vec3_scalarf(tP3, (ttt));
        const plVec3 p5 = pl_add_vec3(p0,p1);
        const plVec3 p6 = pl_add_vec3(p2,p3);
        const plVec3 p7 = pl_add_vec3(p5,p6);
        
        // shift and add next point
        atVerticies[0] = atVerticies[1];
        atVerticies[1] = p7;

        pl__add_3d_line(ptDrawlist, atVerticies[0], atVerticies[1], tOptions);
    }

    // set up last point
    atVerticies[0] = atVerticies[1];
    atVerticies[1] = tP3;
    pl__add_3d_line(ptDrawlist, atVerticies[0], atVerticies[1], tOptions);
}

//-----------------------------------------------------------------------------
// [SECTION] default font stuff
//-----------------------------------------------------------------------------

static uint32_t
pl__draw_decompress_length(const unsigned char* pucInput)
{
    if(pucInput)
        return (pucInput[8] << 24) + (pucInput[9] << 16) + (pucInput[10] << 8) + pucInput[11];
    return 0;
}

static uint32_t
pl__decode85_byte(char c)
{
    return (c >= '\\') ? c - 36 : c -35;
}

static void
pl__decode85(const unsigned char* pucSrc, unsigned char* pucDst)
{
    while (*pucSrc)
    {
        uint32_t uTmp = pl__decode85_byte(pucSrc[0]) + 85 * (pl__decode85_byte(pucSrc[1]) + 85 * (pl__decode85_byte(pucSrc[2]) + 85 * (pl__decode85_byte(pucSrc[3]) + 85 * pl__decode85_byte(pucSrc[4]))));
        pucDst[0] = ((uTmp >> 0) & 0xFF); 
        pucDst[1] = ((uTmp >> 8) & 0xFF); 
        pucDst[2] = ((uTmp >> 16) & 0xFF); 
        pucDst[3] = ((uTmp >> 24) & 0xFF);
        pucSrc += 5;
        pucDst += 4;
    }
}

static void 
pl__draw_match(const unsigned char* pucData, uint32_t uLength)
{
    PL_ASSERT(ptrDOut_ + uLength <= ptrBarrierOutE_);
    if (ptrDOut_ + uLength > ptrBarrierOutE_)
        ptrDOut_ += uLength; 
    else if (pucData < ptrBarrierOutB_)
        ptrDOut_ = ptrBarrierOutE_ + 1;
    else
    {
        while (uLength--)
            *ptrDOut_++ = *pucData++;
    }
}

static void 
pl__draw_lit(const unsigned char* pucData, uint32_t uLength)
{
    PL_ASSERT(ptrDOut_ + uLength <= ptrBarrierOutE_);
    if (ptrDOut_ + uLength > ptrBarrierOutE_)
        ptrDOut_ += uLength;
    else if (pucData < ptrBarrierInB_)
        ptrDOut_ = ptrBarrierOutE_ + 1; 
    else {memcpy(ptrDOut_, pucData, uLength); ptrDOut_ += uLength; }
}

#define MV_IN2_(x) ((pucI[x] << 8) + pucI[(x) + 1])
#define MV_IN3_(x) ((pucI[x] << 16) + MV_IN2_((x) + 1))
#define MV_IN4_(x) ((pucI[x] << 24) + MV_IN3_((x) + 1))

static const unsigned char*
pl__draw_decompress_token(const unsigned char* pucI)
{
    if (*pucI >= 0x20) 
    {
        if(*pucI >= 0x80)
            pl__draw_match(ptrDOut_ - pucI[1] - 1, pucI[0] - 0x80 + 1), pucI += 2;
        else if (*pucI >= 0x40)
            pl__draw_match(ptrDOut_ - (MV_IN2_(0) - 0x4000 + 1), pucI[2] + 1), pucI += 3;
        else /* *pucI >= 0x20 */
            pl__draw_lit(pucI + 1, pucI[0] - 0x20 + 1), pucI += 1 + (pucI[0] - 0x20 + 1);
    } 
    else 
    {
        if(*pucI >= 0x18)
            pl__draw_match(ptrDOut_ - (MV_IN3_(0) - 0x180000 + 1), pucI[3] + 1), pucI += 4;
        else if (*pucI >= 0x10)
            pl__draw_match(ptrDOut_ - (MV_IN3_(0) - 0x100000 + 1), MV_IN2_(3) + 1), pucI += 5;
        else if (*pucI >= 0x08)
            pl__draw_lit(pucI + 2, MV_IN2_(0) - 0x0800 + 1), pucI += 2 + (MV_IN2_(0) - 0x0800 + 1);
        else if (*pucI == 0x07)
            pl__draw_lit(pucI + 3, MV_IN2_(1) + 1), pucI += 3 + (MV_IN2_(1) + 1);
        else if (*pucI == 0x06)
            pl__draw_match(ptrDOut_ - (MV_IN3_(1) + 1), pucI[4] + 1), pucI += 5;
        else if (*pucI == 0x04)
            pl__draw_match(ptrDOut_ - (MV_IN3_(1) + 1), MV_IN2_(4) + 1), pucI += 6;
    }
    return pucI;
}

static uint32_t 
pl__draw_adler32(uint32_t uAdler32, unsigned char* pucBuf, uint32_t uBufLen)
{
    const uint32_t uAdlerMod = 65521;
    uint32_t s1 = uAdler32 & 0xffff;
    uint32_t s2 = uAdler32 >> 16;
    uint32_t uBlocklen = uBufLen % 5552;

    uint32_t i = 0;
    while (uBufLen) 
    {
        for (i = 0; i + 7 < uBlocklen; i += 8) 
        {
            s1 += pucBuf[0], s2 += s1;
            s1 += pucBuf[1], s2 += s1;
            s1 += pucBuf[2], s2 += s1;
            s1 += pucBuf[3], s2 += s1;
            s1 += pucBuf[4], s2 += s1;
            s1 += pucBuf[5], s2 += s1;
            s1 += pucBuf[6], s2 += s1;
            s1 += pucBuf[7], s2 += s1;
            pucBuf += 8;
        }

        for (; i < uBlocklen; ++i)
            s1 += *pucBuf++, s2 += s1;

        s1 %= uAdlerMod;
        s2 %= uAdlerMod;
        uBufLen -= uBlocklen;
        uBlocklen = 5552;
    }
    return (uint32_t)(s2 << 16) + (uint32_t)s1;
}

static uint32_t 
pl__draw_decompress(unsigned char* pucOut, const unsigned char* pucI, uint32_t uLength)
{
    if(pucI == NULL)
        return 0;
    if (MV_IN4_(0) != 0x57bC0000) 
        return 0;
    if (MV_IN4_(4) != 0)
        return 0;

    const uint32_t uOLen = pl__draw_decompress_length(pucI);
    ptrBarrierInB_ = pucI;
    ptrBarrierOutE_ = pucOut + uOLen;
    ptrBarrierOutB_ = pucOut;
    pucI += 16;

    ptrDOut_ = pucOut;
    for (;;) 
    {
        const unsigned char* ptrOldI = pucI;
        pucI = pl__draw_decompress_token(pucI);
        if (pucI == ptrOldI) 
        {
            if (*pucI == 0x05 && pucI[1] == 0xfa) 
            {
                PL_ASSERT(ptrDOut_ == pucOut + uOLen);
                if (ptrDOut_ != pucOut + uOLen) break;
                if (pl__draw_adler32(1, pucOut, uOLen) != (uint32_t) MV_IN4_(2))break;
                return uOLen;
            } 
        }
        PL_ASSERT(ptrDOut_ <= pucOut + uOLen);
        if (ptrDOut_ > pucOut + uOLen)
            break;
    }
    return 0u;
}

// File: 'ProggyClean.ttf' (41208 bytes)
static const char gcPtrDefaultFontCompressed[11980 + 1] =
    "7])#######hV0qs'/###[),##/l:$#Q6>##5[n42>c-TH`->>#/e>11NNV=Bv(*:.F?uu#(gRU.o0XGH`$vhLG1hxt9?W`#,5LsCp#-i>.r$<$6pD>Lb';9Crc6tgXmKVeU2cD4Eo3R/"
    "2*>]b(MC;$jPfY.;h^`IWM9<Lh2TlS+f-s$o6Q<BWH`YiU.xfLq$N;$0iR/GX:U(jcW2p/W*q?-qmnUCI;jHSAiFWM.R*kU@C=GH?a9wp8f$e.-4^Qg1)Q-GL(lf(r/7GrRgwV%MS=C#"
    "`8ND>Qo#t'X#(v#Y9w0#1D$CIf;W'#pWUPXOuxXuU(H9M(1<q-UE31#^-V'8IRUo7Qf./L>=Ke$$'5F%)]0^#0X@U.a<r:QLtFsLcL6##lOj)#.Y5<-R&KgLwqJfLgN&;Q?gI^#DY2uL"
    "i@^rMl9t=cWq6##weg>$FBjVQTSDgEKnIS7EM9>ZY9w0#L;>>#Mx&4Mvt//L[MkA#W@lK.N'[0#7RL_&#w+F%HtG9M#XL`N&.,GM4Pg;-<nLENhvx>-VsM.M0rJfLH2eTM`*oJMHRC`N"
    "kfimM2J,W-jXS:)r0wK#@Fge$U>`w'N7G#$#fB#$E^$#:9:hk+eOe--6x)F7*E%?76%^GMHePW-Z5l'&GiF#$956:rS?dA#fiK:)Yr+`&#0j@'DbG&#^$PG.Ll+DNa<XCMKEV*N)LN/N"
    "*b=%Q6pia-Xg8I$<MR&,VdJe$<(7G;Ckl'&hF;;$<_=X(b.RS%%)###MPBuuE1V:v&cX&#2m#(&cV]`k9OhLMbn%s$G2,B$BfD3X*sp5#l,$R#]x_X1xKX%b5U*[r5iMfUo9U`N99hG)"
    "tm+/Us9pG)XPu`<0s-)WTt(gCRxIg(%6sfh=ktMKn3j)<6<b5Sk_/0(^]AaN#(p/L>&VZ>1i%h1S9u5o@YaaW$e+b<TWFn/Z:Oh(Cx2$lNEoN^e)#CFY@@I;BOQ*sRwZtZxRcU7uW6CX"
    "ow0i(?$Q[cjOd[P4d)]>ROPOpxTO7Stwi1::iB1q)C_=dV26J;2,]7op$]uQr@_V7$q^%lQwtuHY]=DX,n3L#0PHDO4f9>dC@O>HBuKPpP*E,N+b3L#lpR/MrTEH.IAQk.a>D[.e;mc."
    "x]Ip.PH^'/aqUO/$1WxLoW0[iLA<QT;5HKD+@qQ'NQ(3_PLhE48R.qAPSwQ0/WK?Z,[x?-J;jQTWA0X@KJ(_Y8N-:/M74:/-ZpKrUss?d#dZq]DAbkU*JqkL+nwX@@47`5>w=4h(9.`G"
    "CRUxHPeR`5Mjol(dUWxZa(>STrPkrJiWx`5U7F#.g*jrohGg`cg:lSTvEY/EV_7H4Q9[Z%cnv;JQYZ5q.l7Zeas:HOIZOB?G<Nald$qs]@]L<J7bR*>gv:[7MI2k).'2($5FNP&EQ(,)"
    "U]W]+fh18.vsai00);D3@4ku5P?DP8aJt+;qUM]=+b'8@;mViBKx0DE[-auGl8:PJ&Dj+M6OC]O^((##]`0i)drT;-7X`=-H3[igUnPG-NZlo.#k@h#=Ork$m>a>$-?Tm$UV(?#P6YY#"
    "'/###xe7q.73rI3*pP/$1>s9)W,JrM7SN]'/4C#v$U`0#V.[0>xQsH$fEmPMgY2u7Kh(G%siIfLSoS+MK2eTM$=5,M8p`A.;_R%#u[K#$x4AG8.kK/HSB==-'Ie/QTtG?-.*^N-4B/ZM"
    "_3YlQC7(p7q)&](`6_c)$/*JL(L-^(]$wIM`dPtOdGA,U3:w2M-0<q-]L_?^)1vw'.,MRsqVr.L;aN&#/EgJ)PBc[-f>+WomX2u7lqM2iEumMTcsF?-aT=Z-97UEnXglEn1K-bnEO`gu"
    "Ft(c%=;Am_Qs@jLooI&NX;]0#j4#F14;gl8-GQpgwhrq8'=l_f-b49'UOqkLu7-##oDY2L(te+Mch&gLYtJ,MEtJfLh'x'M=$CS-ZZ%P]8bZ>#S?YY#%Q&q'3^Fw&?D)UDNrocM3A76/"
    "/oL?#h7gl85[qW/NDOk%16ij;+:1a'iNIdb-ou8.P*w,v5#EI$TWS>Pot-R*H'-SEpA:g)f+O$%%`kA#G=8RMmG1&O`>to8bC]T&$,n.LoO>29sp3dt-52U%VM#q7'DHpg+#Z9%H[K<L"
    "%a2E-grWVM3@2=-k22tL]4$##6We'8UJCKE[d_=%wI;'6X-GsLX4j^SgJ$##R*w,vP3wK#iiW&#*h^D&R?jp7+/u&#(AP##XU8c$fSYW-J95_-Dp[g9wcO&#M-h1OcJlc-*vpw0xUX&#"
    "OQFKNX@QI'IoPp7nb,QU//MQ&ZDkKP)X<WSVL(68uVl&#c'[0#(s1X&xm$Y%B7*K:eDA323j998GXbA#pwMs-jgD$9QISB-A_(aN4xoFM^@C58D0+Q+q3n0#3U1InDjF682-SjMXJK)("
    "h$hxua_K]ul92%'BOU&#BRRh-slg8KDlr:%L71Ka:.A;%YULjDPmL<LYs8i#XwJOYaKPKc1h:'9Ke,g)b),78=I39B;xiY$bgGw-&.Zi9InXDuYa%G*f2Bq7mn9^#p1vv%#(Wi-;/Z5h"
    "o;#2:;%d&#x9v68C5g?ntX0X)pT`;%pB3q7mgGN)3%(P8nTd5L7GeA-GL@+%J3u2:(Yf>et`e;)f#Km8&+DC$I46>#Kr]]u-[=99tts1.qb#q72g1WJO81q+eN'03'eM>&1XxY-caEnO"
    "j%2n8)),?ILR5^.Ibn<-X-Mq7[a82Lq:F&#ce+S9wsCK*x`569E8ew'He]h:sI[2LM$[guka3ZRd6:t%IG:;$%YiJ:Nq=?eAw;/:nnDq0(CYcMpG)qLN4$##&J<j$UpK<Q4a1]MupW^-"
    "sj_$%[HK%'F####QRZJ::Y3EGl4'@%FkiAOg#p[##O`gukTfBHagL<LHw%q&OV0##F=6/:chIm0@eCP8X]:kFI%hl8hgO@RcBhS-@Qb$%+m=hPDLg*%K8ln(wcf3/'DW-$.lR?n[nCH-"
    "eXOONTJlh:.RYF%3'p6sq:UIMA945&^HFS87@$EP2iG<-lCO$%c`uKGD3rC$x0BL8aFn--`ke%#HMP'vh1/R&O_J9'um,.<tx[@%wsJk&bUT2`0uMv7gg#qp/ij.L56'hl;.s5CUrxjO"
    "M7-##.l+Au'A&O:-T72L]P`&=;ctp'XScX*rU.>-XTt,%OVU4)S1+R-#dg0/Nn?Ku1^0f$B*P:Rowwm-`0PKjYDDM'3]d39VZHEl4,.j']Pk-M.h^&:0FACm$maq-&sgw0t7/6(^xtk%"
    "LuH88Fj-ekm>GA#_>568x6(OFRl-IZp`&b,_P'$M<Jnq79VsJW/mWS*PUiq76;]/NM_>hLbxfc$mj`,O;&%W2m`Zh:/)Uetw:aJ%]K9h:TcF]u_-Sj9,VK3M.*'&0D[Ca]J9gp8,kAW]"
    "%(?A%R$f<->Zts'^kn=-^@c4%-pY6qI%J%1IGxfLU9CP8cbPlXv);C=b),<2mOvP8up,UVf3839acAWAW-W?#ao/^#%KYo8fRULNd2.>%m]UK:n%r$'sw]J;5pAoO_#2mO3n,'=H5(et"
    "Hg*`+RLgv>=4U8guD$I%D:W>-r5V*%j*W:Kvej.Lp$<M-SGZ':+Q_k+uvOSLiEo(<aD/K<CCc`'Lx>'?;++O'>()jLR-^u68PHm8ZFWe+ej8h:9r6L*0//c&iH&R8pRbA#Kjm%upV1g:"
    "a_#Ur7FuA#(tRh#.Y5K+@?3<-8m0$PEn;J:rh6?I6uG<-`wMU'ircp0LaE_OtlMb&1#6T.#FDKu#1Lw%u%+GM+X'e?YLfjM[VO0MbuFp7;>Q&#WIo)0@F%q7c#4XAXN-U&VB<HFF*qL("
    "$/V,;(kXZejWO`<[5?\?ewY(*9=%wDc;,u<'9t3W-(H1th3+G]ucQ]kLs7df($/*JL]@*t7Bu_G3_7mp7<iaQjO@.kLg;x3B0lqp7Hf,^Ze7-##@/c58Mo(3;knp0%)A7?-W+eI'o8)b<"
    "nKnw'Ho8C=Y>pqB>0ie&jhZ[?iLR@@_AvA-iQC(=ksRZRVp7`.=+NpBC%rh&3]R:8XDmE5^V8O(x<<aG/1N$#FX$0V5Y6x'aErI3I$7x%E`v<-BY,)%-?Psf*l?%C3.mM(=/M0:JxG'?"
    "7WhH%o'a<-80g0NBxoO(GH<dM]n.+%q@jH?f.UsJ2Ggs&4<-e47&Kl+f//9@`b+?.TeN_&B8Ss?v;^Trk;f#YvJkl&w$]>-+k?'(<S:68tq*WoDfZu';mM?8X[ma8W%*`-=;D.(nc7/;"
    ")g:T1=^J$&BRV(-lTmNB6xqB[@0*o.erM*<SWF]u2=st-*(6v>^](H.aREZSi,#1:[IXaZFOm<-ui#qUq2$##Ri;u75OK#(RtaW-K-F`S+cF]uN`-KMQ%rP/Xri.LRcB##=YL3BgM/3M"
    "D?@f&1'BW-)Ju<L25gl8uhVm1hL$##*8###'A3/LkKW+(^rWX?5W_8g)a(m&K8P>#bmmWCMkk&#TR`C,5d>g)F;t,4:@_l8G/5h4vUd%&%950:VXD'QdWoY-F$BtUwmfe$YqL'8(PWX("
    "P?^@Po3$##`MSs?DWBZ/S>+4%>fX,VWv/w'KD`LP5IbH;rTV>n3cEK8U#bX]l-/V+^lj3;vlMb&[5YQ8#pekX9JP3XUC72L,,?+Ni&co7ApnO*5NK,((W-i:$,kp'UDAO(G0Sq7MVjJs"
    "bIu)'Z,*[>br5fX^:FPAWr-m2KgL<LUN098kTF&#lvo58=/vjDo;.;)Ka*hLR#/k=rKbxuV`>Q_nN6'8uTG&#1T5g)uLv:873UpTLgH+#FgpH'_o1780Ph8KmxQJ8#H72L4@768@Tm&Q"
    "h4CB/5OvmA&,Q&QbUoi$a_%3M01H)4x7I^&KQVgtFnV+;[Pc>[m4k//,]1?#`VY[Jr*3&&slRfLiVZJ:]?=K3Sw=[$=uRB?3xk48@aeg<Z'<$#4H)6,>e0jT6'N#(q%.O=?2S]u*(m<-"
    "V8J'(1)G][68hW$5'q[GC&5j`TE?m'esFGNRM)j,ffZ?-qx8;->g4t*:CIP/[Qap7/9'#(1sao7w-.qNUdkJ)tCF&#B^;xGvn2r9FEPFFFcL@.iFNkTve$m%#QvQS8U@)2Z+3K:AKM5i"
    "sZ88+dKQ)W6>J%CL<KE>`.d*(B`-n8D9oK<Up]c$X$(,)M8Zt7/[rdkqTgl-0cuGMv'?>-XV1q['-5k'cAZ69e;D_?$ZPP&s^+7])$*$#@QYi9,5P&#9r+$%CE=68>K8r0=dSC%%(@p7"
    ".m7jilQ02'0-VWAg<a/''3u.=4L$Y)6k/K:_[3=&jvL<L0C/2'v:^;-DIBW,B4E68:kZ;%?8(Q8BH=kO65BW?xSG&#@uU,DS*,?.+(o(#1vCS8#CHF>TlGW'b)Tq7VT9q^*^$$.:&N@@"
    "$&)WHtPm*5_rO0&e%K&#-30j(E4#'Zb.o/(Tpm$>K'f@[PvFl,hfINTNU6u'0pao7%XUp9]5.>%h`8_=VYbxuel.NTSsJfLacFu3B'lQSu/m6-Oqem8T+oE--$0a/k]uj9EwsG>%veR*"
    "hv^BFpQj:K'#SJ,sB-'#](j.Lg92rTw-*n%@/;39rrJF,l#qV%OrtBeC6/,;qB3ebNW[?,Hqj2L.1NP&GjUR=1D8QaS3Up&@*9wP?+lo7b?@%'k4`p0Z$22%K3+iCZj?XJN4Nm&+YF]u"
    "@-W$U%VEQ/,,>>#)D<h#`)h0:<Q6909ua+&VU%n2:cG3FJ-%@Bj-DgLr`Hw&HAKjKjseK</xKT*)B,N9X3]krc12t'pgTV(Lv-tL[xg_%=M_q7a^x?7Ubd>#%8cY#YZ?=,`Wdxu/ae&#"
    "w6)R89tI#6@s'(6Bf7a&?S=^ZI_kS&ai`&=tE72L_D,;^R)7[$s<Eh#c&)q.MXI%#v9ROa5FZO%sF7q7Nwb&#ptUJ:aqJe$Sl68%.D###EC><?-aF&#RNQv>o8lKN%5/$(vdfq7+ebA#"
    "u1p]ovUKW&Y%q]'>$1@-[xfn$7ZTp7mM,G,Ko7a&Gu%G[RMxJs[0MM%wci.LFDK)(<c`Q8N)jEIF*+?P2a8g%)$q]o2aH8C&<SibC/q,(e:v;-b#6[$NtDZ84Je2KNvB#$P5?tQ3nt(0"
    "d=j.LQf./Ll33+(;q3L-w=8dX$#WF&uIJ@-bfI>%:_i2B5CsR8&9Z&#=mPEnm0f`<&c)QL5uJ#%u%lJj+D-r;BoF&#4DoS97h5g)E#o:&S4weDF,9^Hoe`h*L+_a*NrLW-1pG_&2UdB8"
    "6e%B/:=>)N4xeW.*wft-;$'58-ESqr<b?UI(_%@[P46>#U`'6AQ]m&6/`Z>#S?YY#Vc;r7U2&326d=w&H####?TZ`*4?&.MK?LP8Vxg>$[QXc%QJv92.(Db*B)gb*BM9dM*hJMAo*c&#"
    "b0v=Pjer]$gG&JXDf->'StvU7505l9$AFvgYRI^&<^b68?j#q9QX4SM'RO#&sL1IM.rJfLUAj221]d##DW=m83u5;'bYx,*Sl0hL(W;;$doB&O/TQ:(Z^xBdLjL<Lni;''X.`$#8+1GD"
    ":k$YUWsbn8ogh6rxZ2Z9]%nd+>V#*8U_72Lh+2Q8Cj0i:6hp&$C/:p(HK>T8Y[gHQ4`4)'$Ab(Nof%V'8hL&#<NEdtg(n'=S1A(Q1/I&4([%dM`,Iu'1:_hL>SfD07&6D<fp8dHM7/g+"
    "tlPN9J*rKaPct&?'uBCem^jn%9_K)<,C5K3s=5g&GmJb*[SYq7K;TRLGCsM-$$;S%:Y@r7AK0pprpL<Lrh,q7e/%KWK:50I^+m'vi`3?%Zp+<-d+$L-Sv:@.o19n$s0&39;kn;S%BSq*"
    "$3WoJSCLweV[aZ'MQIjO<7;X-X;&+dMLvu#^UsGEC9WEc[X(wI7#2.(F0jV*eZf<-Qv3J-c+J5AlrB#$p(H68LvEA'q3n0#m,[`*8Ft)FcYgEud]CWfm68,(aLA$@EFTgLXoBq/UPlp7"
    ":d[/;r_ix=:TF`S5H-b<LI&HY(K=h#)]Lk$K14lVfm:x$H<3^Ql<M`$OhapBnkup'D#L$Pb_`N*g]2e;X/Dtg,bsj&K#2[-:iYr'_wgH)NUIR8a1n#S?Yej'h8^58UbZd+^FKD*T@;6A"
    "7aQC[K8d-(v6GI$x:T<&'Gp5Uf>@M.*J:;$-rv29'M]8qMv-tLp,'886iaC=Hb*YJoKJ,(j%K=H`K.v9HggqBIiZu'QvBT.#=)0ukruV&.)3=(^1`o*Pj4<-<aN((^7('#Z0wK#5GX@7"
    "u][`*S^43933A4rl][`*O4CgLEl]v$1Q3AeF37dbXk,.)vj#x'd`;qgbQR%FW,2(?LO=s%Sc68%NP'##Aotl8x=BE#j1UD([3$M(]UI2LX3RpKN@;/#f'f/&_mt&F)XdF<9t4)Qa.*kT"
    "LwQ'(TTB9.xH'>#MJ+gLq9-##@HuZPN0]u:h7.T..G:;$/Usj(T7`Q8tT72LnYl<-qx8;-HV7Q-&Xdx%1a,hC=0u+HlsV>nuIQL-5<N?)NBS)QN*_I,?&)2'IM%L3I)X((e/dl2&8'<M"
    ":^#M*Q+[T.Xri.LYS3v%fF`68h;b-X[/En'CR.q7E)p'/kle2HM,u;^%OKC-N+Ll%F9CF<Nf'^#t2L,;27W:0O@6##U6W7:$rJfLWHj$#)woqBefIZ.PK<b*t7ed;p*_m;4ExK#h@&]>"
    "_>@kXQtMacfD.m-VAb8;IReM3$wf0''hra*so568'Ip&vRs849'MRYSp%:t:h5qSgwpEr$B>Q,;s(C#$)`svQuF$##-D,##,g68@2[T;.XSdN9Qe)rpt._K-#5wF)sP'##p#C0c%-Gb%"
    "hd+<-j'Ai*x&&HMkT]C'OSl##5RG[JXaHN;d'uA#x._U;.`PU@(Z3dt4r152@:v,'R.Sj'w#0<-;kPI)FfJ&#AYJ&#//)>-k=m=*XnK$>=)72L]0I%>.G690a:$##<,);?;72#?x9+d;"
    "^V'9;jY@;)br#q^YQpx:X#Te$Z^'=-=bGhLf:D6&bNwZ9-ZD#n^9HhLMr5G;']d&6'wYmTFmL<LD)F^%[tC'8;+9E#C$g%#5Y>q9wI>P(9mI[>kC-ekLC/R&CH+s'B;K-M6$EB%is00:"
    "+A4[7xks.LrNk0&E)wILYF@2L'0Nb$+pv<(2.768/FrY&h$^3i&@+G%JT'<-,v`3;_)I9M^AE]CN?Cl2AZg+%4iTpT3<n-&%H%b<FDj2M<hH=&Eh<2Len$b*aTX=-8QxN)k11IM1c^j%"
    "9s<L<NFSo)B?+<-(GxsF,^-Eh@$4dXhN$+#rxK8'je'D7k`e;)2pYwPA'_p9&@^18ml1^[@g4t*[JOa*[=Qp7(qJ_oOL^('7fB&Hq-:sf,sNj8xq^>$U4O]GKx'm9)b@p7YsvK3w^YR-"
    "CdQ*:Ir<($u&)#(&?L9Rg3H)4fiEp^iI9O8KnTj,]H?D*r7'M;PwZ9K0E^k&-cpI;.p/6_vwoFMV<->#%Xi.LxVnrU(4&8/P+:hLSKj$#U%]49t'I:rgMi'FL@a:0Y-uA[39',(vbma*"
    "hU%<-SRF`Tt:542R_VV$p@[p8DV[A,?1839FWdF<TddF<9Ah-6&9tWoDlh]&1SpGMq>Ti1O*H&#(AL8[_P%.M>v^-))qOT*F5Cq0`Ye%+$B6i:7@0IX<N+T+0MlMBPQ*Vj>SsD<U4JHY"
    "8kD2)2fU/M#$e.)T4,_=8hLim[&);?UkK'-x?'(:siIfL<$pFM`i<?%W(mGDHM%>iWP,##P`%/L<eXi:@Z9C.7o=@(pXdAO/NLQ8lPl+HPOQa8wD8=^GlPa8TKI1CjhsCTSLJM'/Wl>-"
    "S(qw%sf/@%#B6;/U7K]uZbi^Oc^2n<bhPmUkMw>%t<)'mEVE''n`WnJra$^TKvX5B>;_aSEK',(hwa0:i4G?.Bci.(X[?b*($,=-n<.Q%`(X=?+@Am*Js0&=3bh8K]mL<LoNs'6,'85`"
    "0?t/'_U59@]ddF<#LdF<eWdF<OuN/45rY<-L@&#+fm>69=Lb,OcZV/);TTm8VI;?%OtJ<(b4mq7M6:u?KRdF<gR@2L=FNU-<b[(9c/ML3m;Z[$oF3g)GAWqpARc=<ROu7cL5l;-[A]%/"
    "+fsd;l#SafT/f*W]0=O'$(Tb<[)*@e775R-:Yob%g*>l*:xP?Yb.5)%w_I?7uk5JC+FS(m#i'k.'a0i)9<7b'fs'59hq$*5Uhv##pi^8+hIEBF`nvo`;'l0.^S1<-wUK2/Coh58KKhLj"
    "M=SO*rfO`+qC`W-On.=AJ56>>i2@2LH6A:&5q`?9I3@@'04&p2/LVa*T-4<-i3;M9UvZd+N7>b*eIwg:CC)c<>nO&#<IGe;__.thjZl<%w(Wk2xmp4Q@I#I9,DF]u7-P=.-_:YJ]aS@V"
    "?6*C()dOp7:WL,b&3Rg/.cmM9&r^>$(>.Z-I&J(Q0Hd5Q%7Co-b`-c<N(6r@ip+AurK<m86QIth*#v;-OBqi+L7wDE-Ir8K['m+DDSLwK&/.?-V%U_%3:qKNu$_b*B-kp7NaD'QdWQPK"
    "Yq[@>P)hI;*_F]u`Rb[.j8_Q/<&>uu+VsH$sM9TA%?)(vmJ80),P7E>)tjD%2L=-t#fK[%`v=Q8<FfNkgg^oIbah*#8/Qt$F&:K*-(N/'+1vMB,u()-a.VUU*#[e%gAAO(S>WlA2);Sa"
    ">gXm8YB`1d@K#n]76-a$U,mF<fX]idqd)<3,]J7JmW4`6]uks=4-72L(jEk+:bJ0M^q-8Dm_Z?0olP1C9Sa&H[d&c$ooQUj]Exd*3ZM@-WGW2%s',B-_M%>%Ul:#/'xoFM9QX-$.QN'>"
    "[%$Z$uF6pA6Ki2O5:8w*vP1<-1`[G,)-m#>0`P&#eb#.3i)rtB61(o'$?X3B</R90;eZ]%Ncq;-Tl]#F>2Qft^ae_5tKL9MUe9b*sLEQ95C&`=G?@Mj=wh*'3E>=-<)Gt*Iw)'QG:`@I"
    "wOf7&]1i'S01B+Ev/Nac#9S;=;YQpg_6U`*kVY39xK,[/6Aj7:'1Bm-_1EYfa1+o&o4hp7KN_Q(OlIo@S%;jVdn0'1<Vc52=u`3^o-n1'g4v58Hj&6_t7$##?M)c<$bgQ_'SY((-xkA#"
    "Y(,p'H9rIVY-b,'%bCPF7.J<Up^,(dU1VY*5#WkTU>h19w,WQhLI)3S#f$2(eb,jr*b;3Vw]*7NH%$c4Vs,eD9>XW8?N]o+(*pgC%/72LV-u<Hp,3@e^9UB1J+ak9-TN/mhKPg+AJYd$"
    "MlvAF_jCK*.O-^(63adMT->W%iewS8W6m2rtCpo'RS1R84=@paTKt)>=%&1[)*vp'u+x,VrwN;&]kuO9JDbg=pO$J*.jVe;u'm0dr9l,<*wMK*Oe=g8lV_KEBFkO'oU]^=[-792#ok,)"
    "i]lR8qQ2oA8wcRCZ^7w/Njh;?.stX?Q1>S1q4Bn$)K1<-rGdO'$Wr.Lc.CG)$/*JL4tNR/,SVO3,aUw'DJN:)Ss;wGn9A32ijw%FL+Z0Fn.U9;reSq)bmI32U==5ALuG&#Vf1398/pVo"
    "1*c-(aY168o<`JsSbk-,1N;$>0:OUas(3:8Z972LSfF8eb=c-;>SPw7.6hn3m`9^Xkn(r.qS[0;T%&Qc=+STRxX'q1BNk3&*eu2;&8q$&x>Q#Q7^Tf+6<(d%ZVmj2bDi%.3L2n+4W'$P"
    "iDDG)g,r%+?,$@?uou5tSe2aN_AQU*<h`e-GI7)?OK2A.d7_c)?wQ5AS@DL3r#7fSkgl6-++D:'A,uq7SvlB$pcpH'q3n0#_%dY#xCpr-l<F0NR@-##FEV6NTF6##$l84N1w?AO>'IAO"
    "URQ##V^Fv-XFbGM7Fl(N<3DhLGF%q.1rC$#:T__&Pi68%0xi_&[qFJ(77j_&JWoF.V735&T,[R*:xFR*K5>>#`bW-?4Ne_&6Ne_&6Ne_&n`kr-#GJcM6X;uM6X;uM(.a..^2TkL%oR(#"
    ";u.T%fAr%4tJ8&><1=GHZ_+m9/#H1F^R#SC#*N=BA9(D?v[UiFY>>^8p,KKF.W]L29uLkLlu/+4T<XoIB&hx=T1PcDaB&;HH+-AFr?(m9HZV)FKS8JCw;SD=6[^/DZUL`EUDf]GGlG&>"
    "w$)F./^n3+rlo+DB;5sIYGNk+i1t-69Jg--0pao7Sm#K)pdHW&;LuDNH@H>#/X-TI(;P>#,Gc>#0Su>#4`1?#8lC?#<xU?#@.i?#D:%@#HF7@#LRI@#P_[@#Tkn@#Xw*A#]-=A#a9OA#"
    "d<F&#*;G##.GY##2Sl##6`($#:l:$#>xL$#B.`$#F:r$#JF.%#NR@%#R_R%#Vke%#Zww%#_-4&#3^Rh%Sflr-k'MS.o?.5/sWel/wpEM0%3'/1)K^f1-d>G21&v(35>V`39V7A4=onx4"
    "A1OY5EI0;6Ibgr6M$HS7Q<)58C5w,;WoA*#[%T*#`1g*#d=#+#hI5+#lUG+#pbY+#tnl+#x$),#&1;,#*=M,#.I`,#2Ur,#6b.-#;w[H#iQtA#m^0B#qjBB#uvTB##-hB#'9$C#+E6C#"
    "/QHC#3^ZC#7jmC#;v)D#?,<D#C8ND#GDaD#KPsD#O]/E#g1A5#KA*1#gC17#MGd;#8(02#L-d3#rWM4#Hga1#,<w0#T.j<#O#'2#CYN1#qa^:#_4m3#o@/=#eG8=#t8J5#`+78#4uI-#"
    "m3B2#SB[8#Q0@8#i[*9#iOn8#1Nm;#^sN9#qh<9#:=x-#P;K2#$%X9#bC+.#Rg;<#mN=.#MTF.#RZO.#2?)4#Y#(/#[)1/#b;L/#dAU/#0Sv;#lY$0#n`-0#sf60#(F24#wrH0#%/e0#"
    "TmD<#%JSMFove:CTBEXI:<eh2g)B,3h2^G3i;#d3jD>)4kMYD4lVu`4m`:&5niUA5@(A5BA1]PBB:xlBCC=2CDLXMCEUtiCf&0g2'tN?PGT4CPGT4CPGT4CPGT4CPGT4CPGT4CPGT4CP"
    "GT4CPGT4CPGT4CPGT4CPGT4CPGT4CP-qekC`.9kEg^+F$kwViFJTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5o,^<-28ZI'O?;xp"
    "O?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xp;7q-#lLYI:xvD=#";

static plFont*
pl_add_default_font(plFontAtlas* ptAtlas)
{

    void* pData = NULL;

    int iCompressedTTFSize = (((int)strlen(gcPtrDefaultFontCompressed) + 4) / 5) * 4;
    void* pCompressedTTF = PL_ALLOC((size_t)iCompressedTTFSize);
    pl__decode85((const unsigned char*)gcPtrDefaultFontCompressed, (unsigned char*)pCompressedTTF);

    const uint32_t uDecompressedSize = pl__draw_decompress_length((const unsigned char*)pCompressedTTF);
    pData = (unsigned char*)PL_ALLOC(uDecompressedSize);
    pl__draw_decompress((unsigned char*)pData, (const unsigned char*)pCompressedTTF, (int)iCompressedTTFSize);

    PL_FREE(pCompressedTTF);

    static const plFontRange tRange = {
        .iFirstCodePoint = 0x0020,
        .uCharCount = 0x00FF - 0x0020
    };

    plFontConfig tFontConfig = {
        .bSdf           = false,
        .fSize          = 13.0f,
        .uHOverSampling = 1,
        .uVOverSampling = 1,
        .ucOnEdgeValue  = 255,
        .iSdfPadding    = 1,
        .ptRanges       = &tRange,
        .uRangeCount    = 1
    };
    
    return pl_add_font_from_memory_ttf(ptAtlas, tFontConfig, pData);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__prepare_draw_command(plDrawLayer2D* ptLayer, plTextureID tTextureID, bool bSdf)
{
    bool bCreateNewCommand = true;

    const plRect tCurrentClip = pl_sb_size(ptLayer->ptDrawlist->_sbtClipStack) > 0 ? pl_sb_top(ptLayer->ptDrawlist->_sbtClipStack) : (plRect){0};

    if(ptLayer->ptLastCommand)
    {
        // check if last command has same texture
        if(ptLayer->ptLastCommand->tTextureId == tTextureID && ptLayer->ptLastCommand->bSdf == bSdf)
        {
            bCreateNewCommand = false;
        }

        // check if last command has same clipping
        if(ptLayer->ptLastCommand->tClip.tMax.x != tCurrentClip.tMax.x ||
            ptLayer->ptLastCommand->tClip.tMax.y != tCurrentClip.tMax.y ||
            ptLayer->ptLastCommand->tClip.tMin.x != tCurrentClip.tMin.x ||
            ptLayer->ptLastCommand->tClip.tMin.y != tCurrentClip.tMin.y)
        {
            bCreateNewCommand = true;
        }
    }

    // new command needed
    if(bCreateNewCommand)
    {
        plDrawCommand tNewdrawCommand = 
        {
            .uVertexOffset = pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer),
            .uIndexOffset  = pl_sb_size(ptLayer->sbuIndexBuffer),
            .uElementCount = 0,
            .tTextureId    = tTextureID,
            .bSdf          = bSdf,
            .tClip         = tCurrentClip
        };
        pl_sb_push(ptLayer->sbtCommandBuffer, tNewdrawCommand);
        
    }
    ptLayer->ptLastCommand = &pl_sb_top(ptLayer->sbtCommandBuffer);
    ptLayer->ptLastCommand->tTextureId = tTextureID;
}

static void
pl__reserve_triangles(plDrawLayer2D* ptLayer, uint32_t uIndexCount, uint32_t uVertexCount)
{
    pl_sb_reserve(ptLayer->ptDrawlist->sbtVertexBuffer, pl_sb_size(ptLayer->ptDrawlist->sbtVertexBuffer) + uVertexCount);
    pl_sb_reserve(ptLayer->sbuIndexBuffer, pl_sb_size(ptLayer->sbuIndexBuffer) + uIndexCount);
    ptLayer->ptLastCommand->uElementCount += uIndexCount; 
    ptLayer->uVertexCount += uVertexCount;
}

static void
pl__add_vertex(plDrawLayer2D* ptLayer, plVec2 tPos, uint32_t uColor, plVec2 tUv)
{

    pl_sb_push(ptLayer->ptDrawlist->sbtVertexBuffer,
        ((plDrawVertex){
            .afPos[0] = tPos.x,
            .afPos[1] = tPos.y,
            .afUv[0]  = tUv.u,
            .afUv[1]  = tUv.v,
            .uColor   = uColor
        })
    );
}

static void
pl__add_index(plDrawLayer2D* ptLayer, uint32_t uVertexStart, uint32_t i0, uint32_t i1, uint32_t i2)
{
    pl_sb_push(ptLayer->sbuIndexBuffer, uVertexStart + i0);
    pl_sb_push(ptLayer->sbuIndexBuffer, uVertexStart + i1);
    pl_sb_push(ptLayer->sbuIndexBuffer, uVertexStart + i2);
}

static const plFontGlyph*
pl__find_glyph(plFont* ptFont, uint32_t c)
{
    const uint32_t uRangeCount = pl_sb_size(ptFont->_sbtRanges);
    for(uint32_t i = 0; i < uRangeCount; i++)
    {
        const plFontRange* ptRange = &ptFont->_sbtRanges[i];
        if (c >= (uint32_t)ptRange->iFirstCodePoint && c < (uint32_t)ptRange->iFirstCodePoint + (uint32_t)ptRange->uCharCount) 
        {
            const plFontGlyph* ptGlyph = &ptFont->_sbtGlyphs[ptFont->_auCodePoints[c]];
            return ptGlyph;
        }
    }

    if(ptFont->_ptFallbackGlyph)
        return ptFont->_ptFallbackGlyph;

    const plUiWChar atFallbackCharacters[] = { (plUiWChar)PL_UNICODE_CODEPOINT_INVALID, (plUiWChar)'?', (plUiWChar)' ' };

    for(uint32_t j = 0; j < 3; j++)
    {
        for(uint32_t i = 0; i < uRangeCount; i++)
        {
            const plFontRange* ptRange = &ptFont->_sbtRanges[i];
            if (atFallbackCharacters[j] >= (uint32_t)ptRange->iFirstCodePoint && atFallbackCharacters[j] < (uint32_t)ptRange->iFirstCodePoint + (uint32_t)ptRange->uCharCount) 
            {
                ptFont->_ptFallbackGlyph = &ptFont->_sbtGlyphs[ptFont->_auCodePoints[atFallbackCharacters[j]]];
                return ptFont->_ptFallbackGlyph;
            }
        }
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_draw_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDrawI tApi = {
        .initialize                 = pl_initialize,
        .cleanup                    = pl_cleanup,
        .request_3d_drawlist        = pl_request_3d_drawlist,
        .return_3d_drawlist         = pl_return_3d_drawlist,
        .new_frame                  = pl_new_draw_3d_frame,
        .add_3d_triangle_filled     = pl__add_3d_triangle_filled,
        .add_3d_circle_xz_filled    = pl__add_3d_circle_xz_filled,
        .add_3d_band_xz_filled      = pl__add_3d_band_xz_filled,
        .add_3d_band_xy_filled      = pl__add_3d_band_xy_filled,
        .add_3d_band_yz_filled      = pl__add_3d_band_yz_filled,
        .add_3d_sphere_filled       = pl__add_3d_sphere_filled,
        .add_3d_cylinder_filled     = pl__add_3d_cylinder_filled,
        .add_3d_cone_filled         = pl__add_3d_cone_filled,
        .add_3d_centered_box_filled = pl__add_3d_centered_box_filled,
        .add_3d_plane_xz_filled     = pl__add_3d_plane_xz_filled,
        .add_3d_plane_xy_filled     = pl__add_3d_plane_xy_filled,
        .add_3d_plane_yz_filled     = pl__add_3d_plane_yz_filled,
        .add_3d_line                = pl__add_3d_line,
        .add_3d_cross               = pl__add_3d_cross,
        .add_3d_transform           = pl__add_3d_transform,
        .add_3d_frustum             = pl__add_3d_frustum,
        .add_3d_sphere              = pl__add_3d_sphere_ex,
        .add_3d_capsule             = pl__add_3d_capsule_ex,
        .add_3d_cylinder            = pl__add_3d_cylinder,
        .add_3d_cone                = pl__add_3d_cone_ex,
        .add_3d_centered_box        = pl__add_3d_centered_box,
        .add_3d_bezier_quad         = pl__add_3d_bezier_quad,
        .add_3d_bezier_cubic        = pl__add_3d_bezier_cubic,
        .add_3d_aabb                = pl__add_3d_aabb,
        .add_3d_circle_xz           = pl__add_3d_circle_xz,
        .add_3d_text                = pl__add_3d_text,
        .request_2d_drawlist        = pl_request_2d_drawlist,
        .return_2d_drawlist         = pl_return_2d_drawlist,
        .prepare_2d_drawlist        = pl_prepare_2d_drawlist,
        .request_2d_layer           = pl_request_2d_layer,
        .return_2d_layer            = pl_return_2d_layer,
        .submit_2d_layer            = pl_submit_2d_layer,
        .prepare_font_atlas         = pl_prepare_font_atlas,
        .create_font_atlas          = pl_create_font_atlas,
        .set_font_atlas             = pl_set_font_atlas,
        .get_current_font_atlas     = pl_get_font_atlas,
        .get_first_font             = pl_get_first_font,
        .cleanup_font_atlas         = pl_cleanup_font_atlas,
        .add_default_font           = pl_add_default_font,
        .add_font_from_file_ttf     = pl_add_font_from_file_ttf,
        .add_font_from_memory_ttf   = pl_add_font_from_memory_ttf,
        .calculate_text_size        = pl_calculate_text_size,
        .calculate_text_bb          = pl_calculate_text_bb,
        .push_clip_rect_pt          = pl_push_clip_rect_pt,
        .push_clip_rect             = pl_push_clip_rect,
        .pop_clip_rect              = pl_pop_clip_rect,
        .get_clip_rect              = pl_get_clip_rect,
        .add_line                   = pl_add_line,
        .add_lines                  = pl_add_lines,
        .add_2d_callback            = pl_add_2d_callback,
        .add_text                   = pl_add_text_ex,
        .add_text_clipped           = pl_add_text_clipped_ex,
        .add_triangle               = pl_add_triangle,
        .add_triangle_filled        = pl_add_triangle_filled,
        .add_triangles_filled       = pl_add_triangles_filled,
        .add_rect_rounded           = pl_add_rect_rounded_ex,
        .add_rect_rounded_filled    = pl_add_rect_rounded_filled_ex,
        .add_rect                   = pl_add_rect,
        .add_rect_filled            = pl_add_rect_filled,
        .add_quad                   = pl_add_quad,
        .add_quad_filled            = pl_add_quad_filled,
        .add_circle                 = pl_add_circle,
        .add_circle_filled          = pl_add_circle_filled,
        .add_polygon                = pl_add_polygon,
        .add_convex_polygon_filled  = pl_add_convex_polygon_filled,
        .add_image                  = pl_add_image,
        .add_image_ex               = pl_add_image_ex,
        .add_image_quad             = pl_add_image_quad,
        .add_image_quad_ex          = pl_add_image_quad_ex,
        .add_bezier_quad            = pl_add_bezier_quad,
        .add_bezier_cubic           = pl_add_bezier_cubic,
        .set_2d_shader              = pl_set_2d_shader
    };
    pl_set_api(ptApiRegistry, plDrawI, &tApi);


    #ifndef PL_UNITY_BUILD
        gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptVfs    = pl_get_api_latest(ptApiRegistry, plVfsI);
    #endif


    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
        gptDrawCtx = ptDataRegistry->get_data("plDrawContext");
    else  // first load
    {
        static plDrawContext tCtx = {0};
        gptDrawCtx = &tCtx;
        ptDataRegistry->set_data("plDrawContext", gptDrawCtx);
    }
}

PL_EXPORT void
pl_unload_draw_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plDrawI* ptApi = pl_get_api_latest(ptApiRegistry, plDrawI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define STB_RECT_PACK_IMPLEMENTATION
    #include "stb_rect_pack.h"
    #undef STB_RECT_PACK_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

    #define STB_TRUETYPE_IMPLEMENTATION
    #include "stb_truetype.h"
    #undef STB_TRUETYPE_IMPLEMENTATION

#endif