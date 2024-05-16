/*
   pl_draw_3d_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] required apis
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_draw_3d_ext.h"
#include "pl_ds.h"
#include "pl_graphics_ext.h"
#include "pl_gpu_allocators_ext.h"
#include "pl_memory.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plDrawVertex3DSolid
{
    float    afPos[3];
    uint32_t uColor;
} plDrawVertex3DSolid;

typedef struct _plDrawVertex3DLine
{
    float    afPos[3];
    float    fDirection;
    float    fThickness;
    float    fMultiply;
    float    afPosOther[3];
    uint32_t uColor;
} plDrawVertex3DLine;

typedef struct _plDrawList3D
{
    plDrawVertex3DSolid* sbtSolidVertexBuffer;
    uint32_t*            sbtSolidIndexBuffer;
    plDrawVertex3DLine*  sbtLineVertexBuffer;
    uint32_t*            sbtLineIndexBuffer;
} plDrawList3D;

typedef struct _plPipelineEntry
{
    plRenderPassHandle tRenderPass;
    uint32_t           uMSAASampleCount;
    plShaderHandle     tRegularPipeline;
    plShaderHandle     tSecondaryPipeline;
    pl3DDrawFlags      tFlags;
    uint32_t           uSubpassIndex;
} plPipelineEntry;

typedef struct _pl3DBufferInfo
{
    // vertex buffer
    plBufferHandle tVertexBuffer;
    uint32_t       uVertexBufferSize;
    uint32_t       uVertexBufferOffset;

    // index buffer

} pl3DBufferInfo;

typedef struct _plDraw3dContext
{
    plGraphics*               ptGraphics;
    plDeviceMemoryAllocatorI* ptStagingUnCachedAllocator;
    plPipelineEntry*          sbtPipelineEntries;

    plPoolAllocator tDrawlistPool;
    char            acPoolBuffer[sizeof(plDrawList3D) * (PL_MAX_3D_DRAWLISTS + 1)];
    plDrawList3D*   aptDrawlists[PL_MAX_3D_DRAWLISTS];
    uint32_t        uDrawlistCount;

    plBufferHandle atIndexBuffer[PL_FRAMES_IN_FLIGHT];
    uint32_t       auIndexBufferSize[PL_FRAMES_IN_FLIGHT];
    uint32_t       auIndexBufferOffset[PL_FRAMES_IN_FLIGHT];
    pl3DBufferInfo at3DBufferInfo[PL_FRAMES_IN_FLIGHT];
    pl3DBufferInfo atLineBufferInfo[PL_FRAMES_IN_FLIGHT];
} plDraw3dContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plDraw3dContext* gptCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] required apis
//-----------------------------------------------------------------------------

static const plDeviceI*        gptDevice        = NULL;
static const plGraphicsI*      gptGfx           = NULL;
static const plGPUAllocatorsI* gptGpuAllocators = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static plBufferHandle         pl__create_staging_buffer(const plBufferDescription*, const char* pcName, uint32_t uIdentifier);
static const plPipelineEntry* pl__get_pipeline         (plRenderPassHandle, uint32_t uMSAASampleCount, pl3DDrawFlags, uint32_t uSubpassIndex);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl_initialize(plGraphics* ptGraphics)
{
    gptCtx->ptGraphics = ptGraphics;
    gptCtx->ptStagingUnCachedAllocator = gptGpuAllocators->get_staging_uncached_allocator(&ptGraphics->tDevice);

    pl_sb_reserve(gptCtx->sbtPipelineEntries, 32);

    // create initial buffers
    const plBufferDescription tIndexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_STAGING,
        .uByteSize = 4096
    };

    const plBufferDescription tVertexBufferDesc = {
        .tUsage    = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STAGING,
        .uByteSize = 4096
    }; 

    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        gptCtx->auIndexBufferSize[i] = 4096;
        gptCtx->at3DBufferInfo[i].uVertexBufferSize = 4096;
        gptCtx->atLineBufferInfo[i].uVertexBufferSize = 4096;
        gptCtx->atIndexBuffer[i] = pl__create_staging_buffer(&tIndexBufferDesc, "3d draw idx buffer", i);
        gptCtx->at3DBufferInfo[i].tVertexBuffer= pl__create_staging_buffer(&tVertexBufferDesc, "3d draw vtx buffer", i);
        gptCtx->atLineBufferInfo[i].tVertexBuffer= pl__create_staging_buffer(&tVertexBufferDesc, "3d line draw vtx buffer", i);
    }

    size_t szBufferSize = sizeof(plDrawList3D) * (PL_MAX_3D_DRAWLISTS + 1);
    pl_pool_allocator_init(&gptCtx->tDrawlistPool, PL_MAX_3D_DRAWLISTS, sizeof(plDrawList3D), 0, &szBufferSize, gptCtx->acPoolBuffer);
}

static void
pl_cleanup(void)
{
    for(uint32_t i = 0u; i < gptCtx->uDrawlistCount; i++)
    {
        plDrawList3D* ptDrawlist = gptCtx->aptDrawlists[i];
        pl_sb_free(ptDrawlist->sbtSolidIndexBuffer);
        pl_sb_free(ptDrawlist->sbtSolidVertexBuffer);
        pl_sb_free(ptDrawlist->sbtLineVertexBuffer);
        pl_sb_free(ptDrawlist->sbtLineIndexBuffer);
    }
    pl_sb_free(gptCtx->sbtPipelineEntries);
}

static plDrawList3D*
pl_request_drawlist(void)
{
    plDrawList3D* ptDrawlist = pl_pool_allocator_alloc(&gptCtx->tDrawlistPool);
    PL_ASSERT(ptDrawlist && "no drawlist available");

    pl_sb_reserve(ptDrawlist->sbtLineIndexBuffer, 1024);
    pl_sb_reserve(ptDrawlist->sbtLineVertexBuffer, 1024);
    pl_sb_reserve(ptDrawlist->sbtSolidIndexBuffer, 1024);
    pl_sb_reserve(ptDrawlist->sbtSolidVertexBuffer, 1024);

    gptCtx->aptDrawlists[gptCtx->uDrawlistCount] = ptDrawlist;
    gptCtx->uDrawlistCount++;
    return ptDrawlist;
}

static void
pl_return_drawlist(plDrawList3D* ptDrawlist)
{

    pl_sb_free(ptDrawlist->sbtLineIndexBuffer);
    pl_sb_free(ptDrawlist->sbtLineVertexBuffer);
    pl_sb_free(ptDrawlist->sbtSolidIndexBuffer);
    pl_sb_free(ptDrawlist->sbtSolidVertexBuffer);

    uint32_t uCurrentIndex = 0;
    for(uint32_t i = 0; i < gptCtx->uDrawlistCount; i++)
    {
        if(gptCtx->aptDrawlists[i] != ptDrawlist) // skip returning drawlist
        {
            plDrawList3D* ptCurrentDrawlist = gptCtx->aptDrawlists[i];
            gptCtx->aptDrawlists[uCurrentIndex] = ptCurrentDrawlist;
            uCurrentIndex++;
        }
    }
    pl_pool_allocator_free(&gptCtx->tDrawlistPool, ptDrawlist);
    gptCtx->uDrawlistCount--;
}

static void
pl_new_draw_3d_frame(void)
{
    // reset 3d drawlists
    for(uint32_t i = 0u; i < gptCtx->uDrawlistCount; i++)
    {
        plDrawList3D* ptDrawlist = gptCtx->aptDrawlists[i];

        pl_sb_reset(ptDrawlist->sbtSolidVertexBuffer);
        pl_sb_reset(ptDrawlist->sbtLineVertexBuffer);
        pl_sb_reset(ptDrawlist->sbtSolidIndexBuffer);    
        pl_sb_reset(ptDrawlist->sbtLineIndexBuffer);    
    }

    // reset buffer offsets
    for(uint32_t i = 0; i < PL_FRAMES_IN_FLIGHT; i++)
    {
        gptCtx->at3DBufferInfo[i].uVertexBufferOffset = 0;
        gptCtx->atLineBufferInfo[i].uVertexBufferOffset = 0;
        gptCtx->auIndexBufferOffset[i] = 0;
    }
}

static void
pl__submit_drawlist(plDrawList3D* ptDrawlist, plRenderEncoder tEncoder, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags, uint32_t uMSAASampleCount)
{
    plGraphics* ptGfx = gptCtx->ptGraphics;

    const plPipelineEntry* ptEntry = pl__get_pipeline(tEncoder.tRenderPassHandle, uMSAASampleCount, tFlags, tEncoder._uCurrentSubpass);

    const float fAspectRatio = fWidth / fHeight;

    // regular 3D
    if(pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) > 0u)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);

        pl3DBufferInfo* ptBufferInfo = &gptCtx->at3DBufferInfo[ptGfx->uCurrentFrameIndex];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexBufferSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
        {

            gptDevice->queue_buffer_for_deletion(&ptGfx->tDevice, ptBufferInfo->tVertexBuffer);

            const plBufferDescription tBufferDesc = {
                .tUsage    = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STAGING,
                .uByteSize = pl_max(ptBufferInfo->uVertexBufferSize * 2, uVtxBufSzNeeded + uAvailableVertexBufferSpace)
            };
            ptBufferInfo->uVertexBufferSize = tBufferDesc.uByteSize;

            ptBufferInfo->tVertexBuffer = pl__create_staging_buffer(&tBufferDesc, "3d draw vtx buffer", ptGfx->uCurrentFrameIndex);
        }

        // vertex GPU data transfer
        plBuffer* ptVertexBuffer = gptDevice->get_buffer(&ptGfx->tDevice, ptBufferInfo->tVertexBuffer);
        char* pucMappedVertexBufferLocation = ptVertexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtSolidVertexBuffer, sizeof(plDrawVertex3DSolid) * pl_sb_size(ptDrawlist->sbtSolidVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = gptCtx->auIndexBufferSize[ptGfx->uCurrentFrameIndex] - gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex];

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
        {
            gptDevice->queue_buffer_for_deletion(&ptGfx->tDevice, gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex]);

            const plBufferDescription tBufferDesc = {
                .tUsage    = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_STAGING,
                .uByteSize = pl_max(gptCtx->auIndexBufferSize[ptGfx->uCurrentFrameIndex] * 2, uIdxBufSzNeeded + uAvailableIndexBufferSpace)
            };
            gptCtx->auIndexBufferSize[ptGfx->uCurrentFrameIndex] = tBufferDesc.uByteSize;

            gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex] = pl__create_staging_buffer(&tBufferDesc, "3d draw idx buffer", ptGfx->uCurrentFrameIndex);
        }

        // index GPU data transfer
        plBuffer* ptIndexBuffer = gptDevice->get_buffer(&ptGfx->tDevice, gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex]);
        char* pucMappedIndexBufferLocation = ptIndexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedIndexBufferLocation[gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex]], ptDrawlist->sbtSolidIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtSolidIndexBuffer));

        plDynamicBinding tSolidDynamicData = gptDevice->allocate_dynamic_data(&ptGfx->tDevice, sizeof(plMat4));
        plMat4* ptSolidDynamicData = (plMat4*)tSolidDynamicData.pcData;
        *ptSolidDynamicData = *ptMVP;

        gptGfx->bind_vertex_buffer(&tEncoder, ptBufferInfo->tVertexBuffer);
        gptGfx->bind_shader(&tEncoder, ptEntry->tRegularPipeline);
        gptGfx->bind_graphics_bind_groups(&tEncoder, ptEntry->tRegularPipeline, 0, 0, NULL, &tSolidDynamicData);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DSolid);
        const int32_t iIndexOffset = gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex] / sizeof(uint32_t);

        const plDrawIndex tDrawIndex = {
            .tIndexBuffer = gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex],
            .uIndexCount = pl_sb_size(ptDrawlist->sbtSolidIndexBuffer),
            .uIndexStart = iIndexOffset,
            .uInstance = 0,
            .uInstanceCount = 1,
            .uVertexStart = iVertexOffset
        };

        gptGfx->draw_indexed(&tEncoder, 1, &tDrawIndex);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex] += uIdxBufSzNeeded;
    }

    // 3D lines
    if(pl_sb_size(ptDrawlist->sbtLineVertexBuffer) > 0u)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu vertex buffer size is adequate
        const uint32_t uVtxBufSzNeeded = sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer);

        pl3DBufferInfo* ptBufferInfo = &gptCtx->atLineBufferInfo[ptGfx->uCurrentFrameIndex];

        // space left in vertex buffer
        const uint32_t uAvailableVertexBufferSpace = ptBufferInfo->uVertexBufferSize - ptBufferInfo->uVertexBufferOffset;

        // grow buffer if not enough room
        if(uVtxBufSzNeeded >= uAvailableVertexBufferSpace)
        {
            gptDevice->queue_buffer_for_deletion(&ptGfx->tDevice, ptBufferInfo->tVertexBuffer);

            const plBufferDescription tBufferDesc = {
                .tUsage    = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_STAGING,
                .uByteSize = pl_max(ptBufferInfo->uVertexBufferSize * 2, uVtxBufSzNeeded + uAvailableVertexBufferSpace)
            };
            ptBufferInfo->uVertexBufferSize = tBufferDesc.uByteSize;

            ptBufferInfo->tVertexBuffer = pl__create_staging_buffer(&tBufferDesc, "3d draw vtx buffer", ptGfx->uCurrentFrameIndex);
        }

        // vertex GPU data transfer
        plBuffer* ptVertexBuffer = gptDevice->get_buffer(&ptGfx->tDevice, ptBufferInfo->tVertexBuffer);
        char* pucMappedVertexBufferLocation = ptVertexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedVertexBufferLocation[ptBufferInfo->uVertexBufferOffset], ptDrawlist->sbtLineVertexBuffer, sizeof(plDrawVertex3DLine) * pl_sb_size(ptDrawlist->sbtLineVertexBuffer));

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // ensure gpu index buffer size is adequate
        const uint32_t uIdxBufSzNeeded = sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer);

        // space left in index buffer
        const uint32_t uAvailableIndexBufferSpace = gptCtx->auIndexBufferSize[ptGfx->uCurrentFrameIndex] - gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex];

        if(uIdxBufSzNeeded >= uAvailableIndexBufferSpace)
        {

            gptDevice->queue_buffer_for_deletion(&ptGfx->tDevice, gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex]);

            const plBufferDescription tBufferDesc = {
                .tUsage    = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_STAGING,
                .uByteSize = pl_max(gptCtx->auIndexBufferSize[ptGfx->uCurrentFrameIndex] * 2, uIdxBufSzNeeded + uAvailableIndexBufferSpace)
            };
            gptCtx->auIndexBufferSize[ptGfx->uCurrentFrameIndex] = tBufferDesc.uByteSize;

            gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex] = pl__create_staging_buffer(&tBufferDesc, "3d draw idx buffer", ptGfx->uCurrentFrameIndex);
        }

        // index GPU data transfer
        plBuffer* ptIndexBuffer = gptDevice->get_buffer(&ptGfx->tDevice, gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex]);
        char* pucMappedIndexBufferLocation = ptIndexBuffer->tMemoryAllocation.pHostMapped;
        memcpy(&pucMappedIndexBufferLocation[gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex]], ptDrawlist->sbtLineIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptDrawlist->sbtLineIndexBuffer));
        
        typedef struct _plLineDynamiceData
        {
            plMat4 tMVP;
            float fAspect;
            int   padding[3];
        } plLineDynamiceData;

        plDynamicBinding tLineDynamicData = gptDevice->allocate_dynamic_data(&ptGfx->tDevice, sizeof(plLineDynamiceData));
        plLineDynamiceData* ptLineDynamicData = (plLineDynamiceData*)tLineDynamicData.pcData;
        ptLineDynamicData->tMVP = *ptMVP;
        ptLineDynamicData->fAspect = fAspectRatio;

        gptGfx->bind_vertex_buffer(&tEncoder, ptBufferInfo->tVertexBuffer);
        gptGfx->bind_shader(&tEncoder, ptEntry->tSecondaryPipeline);
        gptGfx->bind_graphics_bind_groups(&tEncoder, ptEntry->tSecondaryPipeline, 0, 0, NULL, &tLineDynamicData);

        const int32_t iVertexOffset = ptBufferInfo->uVertexBufferOffset / sizeof(plDrawVertex3DLine);
        const int32_t iIndexOffset = gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex] / sizeof(uint32_t);

        const plDrawIndex tDrawIndex = {
            .tIndexBuffer = gptCtx->atIndexBuffer[ptGfx->uCurrentFrameIndex],
            .uIndexCount = pl_sb_size(ptDrawlist->sbtLineIndexBuffer),
            .uIndexStart = iVertexOffset,
            .uInstance = 0,
            .uInstanceCount = 1,
            .uVertexStart = iVertexOffset
        };

        gptGfx->draw_indexed(&tEncoder, 1, &tDrawIndex);
        
        // bump vertex & index buffer offset
        ptBufferInfo->uVertexBufferOffset += uVtxBufSzNeeded;
        gptCtx->auIndexBufferOffset[ptGfx->uCurrentFrameIndex] += uIdxBufSzNeeded;
    }
}

static void
pl__add_triangle_filled(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor)
{

    pl_sb_reserve(ptDrawlist->sbtSolidVertexBuffer, pl_sb_size(ptDrawlist->sbtSolidVertexBuffer) + 3);
    pl_sb_reserve(ptDrawlist->sbtSolidIndexBuffer, pl_sb_size(ptDrawlist->sbtSolidIndexBuffer) + 3);

    const uint32_t uVertexStart = pl_sb_size(ptDrawlist->sbtSolidVertexBuffer);

    uint32_t tU32Color = 0;
    tU32Color = (uint32_t)  (255.0f * tColor.r + 0.5f);
    tU32Color |= (uint32_t) (255.0f * tColor.g + 0.5f) << 8;
    tU32Color |= (uint32_t) (255.0f * tColor.b + 0.5f) << 16;
    tU32Color |= (uint32_t) (255.0f * tColor.a + 0.5f) << 24;

    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP0.x, tP0.y, tP0.z}, tU32Color}));
    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP1.x, tP1.y, tP1.z}, tU32Color}));
    pl_sb_push(ptDrawlist->sbtSolidVertexBuffer, ((plDrawVertex3DSolid){ {tP2.x, tP2.y, tP2.z}, tU32Color}));

    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 0);
    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 1);
    pl_sb_push(ptDrawlist->sbtSolidIndexBuffer, uVertexStart + 2);
}

static void
pl__add_line(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness)
{
    uint32_t tU32Color = 0;
    tU32Color = (uint32_t)  (255.0f * tColor.r + 0.5f);
    tU32Color |= (uint32_t) (255.0f * tColor.g + 0.5f) << 8;
    tU32Color |= (uint32_t) (255.0f * tColor.b + 0.5f) << 16;
    tU32Color |= (uint32_t) (255.0f * tColor.a + 0.5f) << 24;

    pl_sb_reserve(ptDrawlist->sbtLineVertexBuffer, pl_sb_size(ptDrawlist->sbtLineVertexBuffer) + 4);
    pl_sb_reserve(ptDrawlist->sbtLineIndexBuffer, pl_sb_size(ptDrawlist->sbtLineIndexBuffer) + 6);

    plDrawVertex3DLine tNewVertex0 = {
        {tP0.x, tP0.y, tP0.z},
        -1.0f,
        fThickness,
        1.0f,
        {tP1.x, tP1.y, tP1.z},
        tU32Color
    };

    plDrawVertex3DLine tNewVertex1 = {
        {tP1.x, tP1.y, tP1.z},
        -1.0f,
        fThickness,
        -1.0f,
        {tP0.x, tP0.y, tP0.z},
        tU32Color
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
pl__add_point(plDrawList3D* ptDrawlist, plVec3 tP, plVec4 tColor, float fLength, float fThickness)
{
    const plVec3 aatVerticies[6] = {
        {  tP.x - fLength / 2.0f,  tP.y, tP.z},
        {  tP.x + fLength / 2.0f,  tP.y, tP.z},
        {  tP.x,  tP.y - fLength / 2.0f, tP.z},
        {  tP.x,  tP.y + fLength / 2.0f, tP.z},
        {  tP.x,  tP.y, tP.z - fLength / 2.0f},
        {  tP.x,  tP.y, tP.z + fLength / 2.0f}
    };

    pl__add_line(ptDrawlist, aatVerticies[0], aatVerticies[1], tColor, fThickness);
    pl__add_line(ptDrawlist, aatVerticies[2], aatVerticies[3], tColor, fThickness);
    pl__add_line(ptDrawlist, aatVerticies[4], aatVerticies[5], tColor, fThickness);
}

static void
pl__add_transform(plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness)
{

    const plVec3 tOrigin = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, 0.0f});
    const plVec3 tXAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){fLength, 0.0f, 0.0f});
    const plVec3 tYAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, fLength, 0.0f});
    const plVec3 tZAxis  = pl_mul_mat4_vec3(ptTransform, (plVec3){0.0f, 0.0f, fLength});

    pl__add_line(ptDrawlist, tOrigin, tXAxis, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, fThickness);
    pl__add_line(ptDrawlist, tOrigin, tYAxis, (plVec4){0.0f, 1.0f, 0.0f, 1.0f}, fThickness);
    pl__add_line(ptDrawlist, tOrigin, tZAxis, (plVec4){0.0f, 0.0f, 1.0f, 1.0f}, fThickness);
}

static void
pl__add_frustum(plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness)
{
    const float fSmallHeight = tanf(fYFov / 2.0f) * fNearZ;
    const float fSmallWidth  = fSmallHeight * fAspect;
    const float fBigHeight   = tanf(fYFov / 2.0f) * fFarZ;
    const float fBigWidth    = fBigHeight * fAspect;

    const plVec3 atVerticies[8] = {
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth,  fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fSmallWidth, -fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth, -fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fSmallWidth,  fSmallHeight, fNearZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,    fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){  fBigWidth,   -fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,   -fBigHeight,   fFarZ}),
        pl_mul_mat4_vec3(ptTransform, (plVec3){ -fBigWidth,    fBigHeight,   fFarZ})
    };

    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[1], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[1], atVerticies[2], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[2], atVerticies[3], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[3], atVerticies[0], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[4], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[1], atVerticies[5], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[2], atVerticies[6], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[3], atVerticies[7], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[4], atVerticies[5], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[5], atVerticies[6], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[6], atVerticies[7], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[7], atVerticies[4], tColor, fThickness);
}

static void
pl__add_centered_box(plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness)
{
    const plVec3 tWidthVec  = {fWidth / 2.0f, 0.0f, 0.0f};
    const plVec3 tHeightVec = {0.0f, fHeight / 2.0f, 0.0f};
    const plVec3 tDepthVec  = {0.0f, 0.0f, fDepth / 2.0f};

    const plVec3 atVerticies[8] = {
        {  tCenter.x - fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z - fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x - fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y - fHeight / 2.0f, tCenter.z + fDepth / 2.0f},
        {  tCenter.x + fWidth / 2.0f,  tCenter.y + fHeight / 2.0f, tCenter.z + fDepth / 2.0f}
    };

    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[1], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[1], atVerticies[2], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[2], atVerticies[3], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[3], atVerticies[0], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[4], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[1], atVerticies[5], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[2], atVerticies[6], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[3], atVerticies[7], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[4], atVerticies[5], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[5], atVerticies[6], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[6], atVerticies[7], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[7], atVerticies[4], tColor, fThickness);
}

static void
pl__add_aabb(plDrawList3D* ptDrawlist, plVec3 tMin, plVec3 tMax, plVec4 tColor, float fThickness)
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

    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[1], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[1], atVerticies[2], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[2], atVerticies[3], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[3], atVerticies[0], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[4], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[1], atVerticies[5], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[2], atVerticies[6], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[3], atVerticies[7], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[4], atVerticies[5], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[5], atVerticies[6], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[6], atVerticies[7], tColor, fThickness);
    pl__add_line(ptDrawlist, atVerticies[7], atVerticies[4], tColor, fThickness);
}

static void
pl__add_bezier_quad(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments)
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

        pl__add_line(ptDrawlist, atVerticies[0], atVerticies[1], tColor, fThickness);
    }

    // set up last point
    atVerticies[0] = atVerticies[1];
    atVerticies[1] = tP2;
    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[1], tColor, fThickness);
}

static void
pl__add_bezier_cubic(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments)
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

        pl__add_line(ptDrawlist, atVerticies[0], atVerticies[1], tColor, fThickness);
    }

    // set up last point
    atVerticies[0] = atVerticies[1];
    atVerticies[1] = tP3;
    pl__add_line(ptDrawlist, atVerticies[0], atVerticies[1], tColor, fThickness);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static plBufferHandle
pl__create_staging_buffer(const plBufferDescription* ptDesc, const char* pcName, uint32_t uIdentifier)
{
    // for convience
    plDevice* ptDevice = &gptCtx->ptGraphics->tDevice;

    // create buffer
    plTempAllocator tTempAllocator = {0};
    const plBufferHandle tHandle = gptDevice->create_buffer(ptDevice, ptDesc, pl_temp_allocator_sprintf(&tTempAllocator, "%s: %u", pcName, uIdentifier));
    pl_temp_allocator_reset(&tTempAllocator);

    // retrieve new buffer
    plBuffer* ptBuffer = gptDevice->get_buffer(ptDevice, tHandle);

    // allocate memory
    const plDeviceMemoryAllocation tAllocation = gptCtx->ptStagingUnCachedAllocator->allocate(gptCtx->ptStagingUnCachedAllocator->ptInst, 
        ptBuffer->tMemoryRequirements.uMemoryTypeBits,
        ptBuffer->tMemoryRequirements.ulSize,
        ptBuffer->tMemoryRequirements.ulAlignment,
        pl_temp_allocator_sprintf(&tTempAllocator, "%s: %u", pcName, uIdentifier));

    // bind memory
    gptDevice->bind_buffer_to_memory(ptDevice, tHandle, &tAllocation);
    pl_temp_allocator_free(&tTempAllocator);
    return tHandle;
}

static const plPipelineEntry*
pl__get_pipeline(plRenderPassHandle tRenderPass, uint32_t uMSAASampleCount, pl3DDrawFlags tFlags, uint32_t uSubpassIndex)
{
    // check if pipeline exists
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtPipelineEntries); i++)
    {
        const plPipelineEntry* ptEntry = &gptCtx->sbtPipelineEntries[i];
        if(ptEntry->tRenderPass.uIndex == tRenderPass.uIndex && ptEntry->uMSAASampleCount == uMSAASampleCount && ptEntry->tFlags == tFlags && ptEntry->uSubpassIndex == uSubpassIndex)
        {
            return ptEntry;
        }
    }

    pl_sb_add(gptCtx->sbtPipelineEntries);
    plPipelineEntry* ptEntry = &gptCtx->sbtPipelineEntries[pl_sb_size(gptCtx->sbtPipelineEntries) - 1];
    ptEntry->tFlags = tFlags;
    ptEntry->tRenderPass = tRenderPass;
    ptEntry->uMSAASampleCount = uMSAASampleCount;
    ptEntry->uSubpassIndex = uSubpassIndex;

    uint64_t ulCullMode = PL_CULL_MODE_NONE;
    if(tFlags & PL_PIPELINE_FLAG_CULL_FRONT)
        ulCullMode |= PL_CULL_MODE_CULL_FRONT;
    if(tFlags & PL_PIPELINE_FLAG_CULL_BACK)
        ulCullMode |= PL_CULL_MODE_CULL_BACK;

    const plShaderDescription t3DShaderDesc = {

        #ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/draw_3d.metal",
        .pcPixelShader = "../shaders/metal/draw_3d.metal",
        #else
        .pcVertexShader = "draw_3d.vert.spv",
        .pcPixelShader = "draw_3d.frag.spv",
        #endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = tFlags & PL_PIPELINE_FLAG_DEPTH_WRITE,
            .ulDepthMode          = tFlags & PL_PIPELINE_FLAG_DEPTH_TEST ? PL_COMPARE_MODE_LESS : PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = ulCullMode,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tVertexBufferBinding = {
            .uByteStride = sizeof(float) * 4,
            .atAttributes = {
                {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                {.uByteOffset = sizeof(float) * 3, .tFormat = PL_FORMAT_R8G8B8A8_UNORM},
            }
        },
        .uConstantCount = 0,
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
        .uBlendStateCount = 1,
        .tRenderPassLayout = gptCtx->ptGraphics->sbtRenderPassesCold[tRenderPass.uIndex].tDesc.tLayout,
        .uSubpassIndex = uSubpassIndex,
        .uBindGroupLayoutCount = 0,
    };

    const plShaderDescription t3DLineShaderDesc = {

        #ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/draw_3d_line.metal",
        .pcPixelShader = "../shaders/metal/draw_3d_line.metal",
        #else
        .pcVertexShader = "draw_3d_line.vert.spv",
        .pcPixelShader = "draw_3d.frag.spv",
        #endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = tFlags & PL_PIPELINE_FLAG_DEPTH_WRITE,
            .ulDepthMode          = tFlags & PL_PIPELINE_FLAG_DEPTH_TEST ? PL_COMPARE_MODE_LESS : PL_COMPARE_MODE_ALWAYS,
            .ulCullMode           = ulCullMode,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tVertexBufferBinding = {
            .uByteStride = sizeof(float) * 10,
            .atAttributes = {
                {.uByteOffset = 0,                 .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                {.uByteOffset = sizeof(float) * 3, .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                {.uByteOffset = sizeof(float) * 6, .tFormat = PL_FORMAT_R32G32B32_FLOAT},
                {.uByteOffset = sizeof(float) * 9, .tFormat = PL_FORMAT_R8G8B8A8_UNORM},
            }
        },
        .uConstantCount = 0,
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
        .uBlendStateCount = 1,
        .tRenderPassLayout = gptCtx->ptGraphics->sbtRenderPassesCold[tRenderPass.uIndex].tDesc.tLayout,
        .uSubpassIndex = uSubpassIndex,
        .uBindGroupLayoutCount = 0,
    };

    ptEntry->tRegularPipeline = gptDevice->create_shader(&gptCtx->ptGraphics->tDevice, &t3DShaderDesc);
    ptEntry->tSecondaryPipeline = gptDevice->create_shader(&gptCtx->ptGraphics->tDevice, &t3DLineShaderDesc);
    return ptEntry;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static const plDraw3dI*
pl_load_draw_3d_api(void)
{
    static const plDraw3dI tApi = {
        .initialize           = pl_initialize,
        .cleanup              = pl_cleanup,
        .request_drawlist     = pl_request_drawlist,
        .return_drawlist      = pl_return_drawlist,
        .submit_drawlist      = pl__submit_drawlist,
        .new_frame            = pl_new_draw_3d_frame,
        .add_triangle_filled  = pl__add_triangle_filled,
        .add_line             = pl__add_line,
        .add_point            = pl__add_point,
        .add_transform        = pl__add_transform,
        .add_frustum          = pl__add_frustum,
        .add_centered_box     = pl__add_centered_box,
        .add_bezier_quad      = pl__add_bezier_quad,
        .add_bezier_cubic     = pl__add_bezier_cubic,
        .add_aabb             = pl__add_aabb
    };
    return &tApi;
}

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    gptDevice = ptApiRegistry->first(PL_API_DEVICE);
    gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
    gptGpuAllocators = ptApiRegistry->first(PL_API_GPU_ALLOCATORS);
    if(bReload)
    {
        gptCtx = ptDataRegistry->get_data("plDraw3dContext");
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_DRAW_3D), pl_load_draw_3d_api());
    }
    else
    {
        ptApiRegistry->add(PL_API_DRAW_3D, pl_load_draw_3d_api());

        static plDraw3dContext tCtx = {0};
        gptCtx = &tCtx;
        ptDataRegistry->set_data("plDraw3dContext", gptCtx);
    }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
}