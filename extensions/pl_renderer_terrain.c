/*
   pl_renderer_terrain.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] job system tasks
// [SECTION] resource creation helpers
// [SECTION] culling
// [SECTION] scene render helpers
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_renderer_internal.h"

#define PL_REQUEST_QUEUE_SIZE 100

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTerrainResidencyNode   plTerrainResidencyNode;
typedef struct _plTerrainReplacementNode plTerrainReplacementNode;
typedef struct _plTerrainContext         plTerrainContext;

typedef struct _plTerrainResidencyNode
{
    plTerrainResidencyNode* ptNext;
    plTerrainResidencyNode* ptPrev;
    plTerrainChunk*         ptChunk;
    uint64_t                uFrameRequested;
} plTerrainResidencyNode;

typedef struct _plChunkFileData
{
    plTerrainChunkFile tFile;
    char               acPakFileName[256];
} plChunkFileData;

typedef struct _plTerrain
{
    plRenderAttachmentInfo    tRenderPassLayoutHandle;
    plChunkFileData*          sbtChunkFiles;
    plAssetHandle             tAsset;
    plVec2                    tTopLeftGlobal;
    plVec2                    tBottomRightGlobal;
    uint32_t                  uTileCount;
    plTerrainProcessTileInfo* atTiles;
    
    plTerrainResidencyNode tRequestQueue;
    plTerrainResidencyNode atRequests[PL_REQUEST_QUEUE_SIZE];
    uint32_t*              sbuFreeRequests;

    plTerrainChunk tReplacementQueue;

    plBufferHandle tIndexBuffer;
    plFreeList     tIndexBufferManager;
    
    plBufferHandle tVertexBuffer;
    plFreeList     tVertexBufferManager;
} plTerrain;

//-----------------------------------------------------------------------------
// [SECTION] internal helpers (rendering)
//-----------------------------------------------------------------------------

// rendering
static void pl__handle_residency             (plTerrain*);
static void pl__request_residency            (plTerrain*, plTerrainChunk*);
static void pl__touch_chunk                  (plTerrain*, plTerrainChunk*);
static void pl__make_unresident              (plTerrain*, plTerrainChunk*);
static void pl__remove_from_replacement_queue(plTerrain*, plTerrainChunk*);

static void pl__render_chunk(plScene*, plTerrain*, const plCamera*, plCommandBuffer*, plTerrainChunk*, plTerrainChunkFile*, uint32_t, float fViewportWidth);
static void pl__render_chunk_shadow(plScene*, plTerrain*, const plCamera*, plCommandBuffer*, plTerrainChunk*, plTerrainChunkFile*, uint32_t uGlobalIndex, float fShadowViewportWidth, float fShadowViewportHeight);
static void pl__free_chunk_until(plTerrain*, uint64_t idx_bytes_needed, uint64_t vtx_bytes_needed);


static inline bool pl__is_leaf_resident(const plTerrainChunk* c)
{
    if (!c->aptChildren[0]) return true; // no children in tree
    return !(c->aptChildren[0]->ptIndexHole ||
             c->aptChildren[1]->ptIndexHole ||
             c->aptChildren[2]->ptIndexHole ||
             c->aptChildren[3]->ptIndexHole);
}

static inline bool
pl__chunk_is_resident(const plTerrainChunk* ptChunk)
{
    return ptChunk && ptChunk->ptIndexHole && ptChunk->ptVertexHole && ptChunk->uIndexCount != 0;
}

static void
pl__defer_residency_head(plTerrain* ptTerrain)
{
    plTerrainResidencyNode* ptHead = ptTerrain->tRequestQueue.ptNext;
    plTerrainResidencyNode* ptTail;
    if (!ptHead || !ptHead->ptNext)
        return;
    ptTerrain->tRequestQueue.ptNext = ptHead->ptNext;
    ptHead->ptNext->ptPrev = NULL;
    ptTail = ptTerrain->tRequestQueue.ptNext;
    while (ptTail->ptNext)
        ptTail = ptTail->ptNext;
    ptTail->ptNext = ptHead;
    ptHead->ptPrev = ptTail;
    ptHead->ptNext = NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_chlod_load_chunk_file(plTerrain* ptTerrain, const char* pcPath)
{
    plChunkFileData tChunkFileData = {0};
    uint32_t uChunkFileID = pl_sb_size(ptTerrain->sbtChunkFiles);

    if (!gptTerrain->load_chunk_file(pcPath, &tChunkFileData.tFile, uChunkFileID))
        return false;

    for(uint32_t i = 0; i < tChunkFileData.tFile.uChunkCount; i++)
    {
        tChunkFileData.tFile.atChunks[i].uIndex = i;
        tChunkFileData.tFile.atChunks[i].tUVScale.x = 1.0f;
        tChunkFileData.tFile.atChunks[i].tUVScale.y = 1.0f;
    }
    pl_sb_push(ptTerrain->sbtChunkFiles, tChunkFileData);
    return true;
}

static plTerrain*
pl__renderer_terrain_create(plCommandBuffer* ptCmdBuffer, plAssetHandle tAsset)
{
    plTerrain* ptTerrain = PL_ALLOC(sizeof(plTerrain));
    memset(ptTerrain, 0, sizeof(plTerrain));

    ptTerrain->tRenderPassLayoutHandle = gptData->tRenderPassLayout;
    ptTerrain->tAsset = tAsset;

    pl_sb_resize(ptTerrain->sbuFreeRequests, PL_REQUEST_QUEUE_SIZE);

    for(uint32_t i = 0; i < PL_REQUEST_QUEUE_SIZE; i++) 
    {
        ptTerrain->sbuFreeRequests[i] = i;
    }

    gptFreeList->create(268435456 * 2, 256, &ptTerrain->tVertexBufferManager);
    gptFreeList->create(268435456 * 2, 256, &ptTerrain->tIndexBufferManager);

    plDevice* ptDevice = gptData->ptDevice;

    const plBufferDesc tVertexBufferDesc = {
        .eUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize  = ptTerrain->tVertexBufferManager.uSize,
        .pcDebugName = "terrain vertex buffer"
    };
    gptStarter->create_buffer(&tVertexBufferDesc, NULL, &ptTerrain->tVertexBuffer);

    // create index buffer
    const plBufferDesc tIndexBufferDesc = {
        .eUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER,
        .szByteSize  = ptTerrain->tIndexBufferManager.uSize,
        .pcDebugName = "terrain index buffer"
    };
    gptStarter->create_buffer(&tIndexBufferDesc, NULL, &ptTerrain->tIndexBuffer);

    plTerrainAsset* ptTerrainAsset = gptAsset->get_data(tAsset);

    {
        float fX = ptTerrainAsset->atTiles[0].tCenter.x;
        float fY = ptTerrainAsset->atTiles[0].tCenter.z;
        ptTerrain->tTopLeftGlobal.x = fX - 0.5f * (float)ptTerrainAsset->uSize * ptTerrainAsset->fMetersPerPixel;
        ptTerrain->tTopLeftGlobal.y = fY - 0.5f * (float)ptTerrainAsset->uSize * ptTerrainAsset->fMetersPerPixel;
    }

    {
        float fX = ptTerrainAsset->atTiles[ptTerrainAsset->uTileCount - 1].tCenter.x;
        float fY = ptTerrainAsset->atTiles[ptTerrainAsset->uTileCount - 1].tCenter.z;
        ptTerrain->tBottomRightGlobal.x = fX - 0.5f * (float)ptTerrainAsset->uSize * ptTerrainAsset->fMetersPerPixel;
        ptTerrain->tBottomRightGlobal.y = fY - 0.5f * (float)ptTerrainAsset->uSize * ptTerrainAsset->fMetersPerPixel;
    }

    for(uint32_t k = 0; k < ptTerrainAsset->uTileCount; k++)
    {
        uint32_t i = k % ptTerrainAsset->uHorizontalTiles;
        uint32_t j = (k - i) / ptTerrainAsset->uVerticalTiles;

        const char* pcAssetPath = gptAsset->get_path(ptTerrainAsset->atTiles[k].tHeightmap);

        char acFileNameOnly[256] = {0};
        pl_str_get_file_name_only(pcAssetPath, acFileNameOnly, 256);

        char acCacheFile[256] = {0};
        pl_sprintf(acCacheFile, "/cache/terrain/%s.chu", acFileNameOnly);

        pl_chlod_load_chunk_file(ptTerrain, acCacheFile);
    }

    ptTerrain->atTiles = PL_ALLOC(sizeof(plTerrainProcessTileInfo) * ptTerrainAsset->uTileCount);
    memset(ptTerrain->atTiles, 0, sizeof(plTerrainProcessTileInfo) * ptTerrainAsset->uTileCount);
    memcpy(ptTerrain->atTiles, ptTerrainAsset->atTiles, sizeof(plTerrainProcessTileInfo) * ptTerrainAsset->uTileCount);
    ptTerrain->uTileCount = ptTerrainAsset->uTileCount;

    for(uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
        pl__request_residency(ptTerrain, &ptTerrain->sbtChunkFiles[i].tFile.atChunks[0]);
    return ptTerrain;
}

static void
pl__renderer_terrain_destroy(plTerrain* ptTerrain)
{
    plDevice* ptDevice = gptData->ptDevice;

    for(uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
    {
        gptTerrain->unload_chunk_file(&ptTerrain->sbtChunkFiles[i].tFile);
        PL_FREE(ptTerrain->sbtChunkFiles[i].tFile.atChunks);
        ptTerrain->sbtChunkFiles[i].tFile.atChunks = NULL;
        ptTerrain->sbtChunkFiles[i].tFile.uChunkCount = 0;
        ptTerrain->sbtChunkFiles[i].tFile.fMaxBaseError = 0.0f;
        ptTerrain->sbtChunkFiles[i].tFile.iTreeDepth = 0;
    }

    PL_FREE(ptTerrain->atTiles);
    ptTerrain->atTiles = NULL;

    // cleanup our resources
    gptGfx->queue_buffer_for_deletion(ptDevice, ptTerrain->tVertexBuffer);
    gptGfx->queue_buffer_for_deletion(ptDevice, ptTerrain->tIndexBuffer);
    gptFreeList->cleanup(&ptTerrain->tVertexBufferManager);
    gptFreeList->cleanup(&ptTerrain->tIndexBufferManager);

    pl_sb_free(ptTerrain->sbuFreeRequests);
    pl_sb_free(ptTerrain->sbtChunkFiles);
    PL_FREE(ptTerrain);
}

static void
pl__unload_children(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    if(ptChunk->aptChildren[0] == NULL)
        return;
    if(ptChunk->aptChildren[0]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[0]);
    if(ptChunk->aptChildren[1]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[1]);
    if(ptChunk->aptChildren[2]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[2]);
    if(ptChunk->aptChildren[3]->ptIndexHole) pl__make_unresident(ptTerrain, ptChunk->aptChildren[3]);    
}

static inline bool
pl__all_children_resident(plTerrainChunk* ptChunk)
{
    for(uint32_t i = 0; i < 4; i++)
    {
        if(!pl__chunk_is_resident(ptChunk->aptChildren[i]))
            return false;
    }
    return true;
}

// Free from LRU tail until we have at least the requested bytes for BOTH pools.
// Never evict level 0, and never evict non-leaves (chunks with resident children).
static void
pl__free_chunk_until(plTerrain* P, uint64_t idx_bytes_needed, uint64_t vtx_bytes_needed)
{
    // Walk to tail (oldest / least-recently-used)
    plTerrainChunk* tail = P->tReplacementQueue.ptNext;
    if(!tail)
    {
        PL_LOG_WARN_API(gptLog, gptData->uLogChannelTerrain, "Couldn't free chunks (replacement list empty)");
        return;
    }

    while (tail->ptNext)
        tail = tail->ptNext;

    uint64_t freed_idx = 0;
    uint64_t freed_vtx = 0;

    // Pass 1: strictly leaf, non-root, LRU → MRU until enough bytes
    for (plTerrainChunk* c = tail; c; c = c->ptPrev)
    {
        plTerrainChunk* ptPrev = c->ptPrev;

        if(pl__chunk_is_resident(c) && c->ptParent != NULL && pl__is_leaf_resident(c))
        {
            freed_idx += c->ptIndexHole->uSize;
            freed_vtx += c->ptVertexHole->uSize;

            pl__make_unresident(P, c);

            if(freed_idx >= idx_bytes_needed &&
            freed_vtx >= vtx_bytes_needed)
                return;
        }

        c = ptPrev;
    }

    // If we reached here, nothing could be evicted (only root is resident)
    PL_LOG_WARN_API(gptLog, gptData->uLogChannelTerrain, "Eviction failed: only root or no resident chunks available");
}

static void
pl__remove_from_residency_queue(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    plTerrainResidencyNode* ptCurrentRequest = ptTerrain->tRequestQueue.ptNext;

    plTerrainResidencyNode* ptExistingRequest = NULL;
    plTerrainResidencyNode* ptLastRequest = NULL;

    while(ptCurrentRequest)
    {
        if(ptCurrentRequest->ptChunk == ptChunk)
        {
            ptExistingRequest = ptCurrentRequest;
            break;
        }
        ptCurrentRequest = ptCurrentRequest->ptNext;
    }

    if(ptExistingRequest)
    {

        // remove node
        if(ptExistingRequest->ptPrev)
            ptExistingRequest->ptPrev->ptNext = ptExistingRequest->ptNext;
        else
            ptTerrain->tRequestQueue.ptNext = ptExistingRequest->ptNext;

        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;

        uint32_t uIndex = (uint32_t)(ptExistingRequest - &ptTerrain->atRequests[0]);
        memset(ptExistingRequest, 0, sizeof(*ptExistingRequest));
        pl_sb_push(ptTerrain->sbuFreeRequests, uIndex);
    }
}


// Helper: clamp integer to a range
static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void
pl_prepare_terrain(plTerrain* ptTerrain)
{
    if(ptTerrain->tRequestQueue.ptNext)
    {
        gptScreenLog->add_message_ex(294, 10.0, PL_COLOR_32_YELLOW, 1.0f, "Stream Active");
        pl__handle_residency(ptTerrain);
    }
    else
    {
        gptScreenLog->add_message_ex(294, 10.0, PL_COLOR_32_RED, 1.0f, "Stream Inactive");
    }
}

static void
pl__make_unresident(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    pl__remove_from_residency_queue(ptTerrain, ptChunk);
    pl__remove_from_replacement_queue(ptTerrain, ptChunk);
    if(pl__chunk_is_resident(ptChunk))
    {
        PL_LOG_DEBUG_API(gptLog, gptData->uLogChannelTerrain, "Made chunk unresident");
        if(ptChunk->ptIndexHole)
            gptFreeList->return_node(&ptTerrain->tIndexBufferManager, ptChunk->ptIndexHole);
        if(ptChunk->ptVertexHole)
            gptFreeList->return_node(&ptTerrain->tVertexBufferManager, ptChunk->ptVertexHole);
        ptChunk->uIndexCount = 0;
        ptChunk->uLastFrameUsed = 0;
        ptChunk->ptIndexHole = NULL;
        ptChunk->ptVertexHole = NULL;
        ptChunk->ptVertexHole = NULL;
        if(ptChunk->aptChildren[0] != NULL)
        {
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[0]);
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[1]);
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[2]);
            pl__make_unresident(ptTerrain, ptChunk->aptChildren[3]);
        }
    }
}

static bool
pl__read_chunk_mesh(const char* pcPath, size_t szLocation, uint64_t uMaxVertexBytes, uint64_t uMaxIndexBytes,
                    plTerrainVertex** pptVertices, uint32_t** ppuIndices, uint32_t* puVertexCount, uint32_t* puIndexCount)
{
    FILE* ptFile = NULL;
    plTerrainVertex* ptVertices = NULL;
    uint32_t* puIndices = NULL;
    uint32_t uVertices = 0, uIndices = 0;
    uint64_t uVertexBytes, uIndexBytes, uRemaining;
    const size_t szHeader = sizeof(plVec3) * 2 + sizeof(int) * 4;
    long lFileSize;
    bool bOK = false;

    *pptVertices = NULL;
    *ppuIndices = NULL;
    *puVertexCount = *puIndexCount = 0;
    if (!pcPath || szHeader > (size_t)LONG_MAX || szLocation > (size_t)LONG_MAX - szHeader)
        return false;

    ptFile = fopen(pcPath, "rb");
    if (!ptFile || fseek(ptFile, 0, SEEK_END) != 0)
        goto cleanup;

    lFileSize = ftell(ptFile);
    if (lFileSize < 0 || (uint64_t)lFileSize < (uint64_t)szLocation + szHeader)
        goto cleanup;

    uRemaining = (uint64_t)lFileSize - szLocation - szHeader;
    if (fseek(ptFile, (long)(szLocation + szHeader), SEEK_SET) != 0 || uRemaining < sizeof(uint32_t) || fread(&uVertices, sizeof(uVertices), 1, ptFile) != 1)
        goto cleanup;
        
    uRemaining -= sizeof(uint32_t);
    uVertexBytes = (uint64_t)uVertices * sizeof(plTerrainVertex);
    if (uVertices == 0 || uVertexBytes > uMaxVertexBytes || uVertexBytes > (uint64_t)SIZE_MAX || uVertexBytes > uRemaining || uRemaining - uVertexBytes < sizeof(uint32_t))
        goto cleanup;

    ptVertices = PL_ALLOC((size_t)uVertexBytes);
    if (fread(ptVertices, 1, (size_t)uVertexBytes, ptFile) != (size_t)uVertexBytes)
        goto cleanup;
    uRemaining -= uVertexBytes;

    if (fread(&uIndices, sizeof(uIndices), 1, ptFile) != 1)
        goto cleanup;

    uRemaining -= sizeof(uint32_t);
    uIndexBytes = (uint64_t)uIndices * sizeof(uint32_t);
    if (uIndices == 0 || uIndexBytes > uMaxIndexBytes || uIndexBytes > (uint64_t)SIZE_MAX || uIndexBytes > uRemaining)
        goto cleanup;

    puIndices = PL_ALLOC((size_t)uIndexBytes);
    if (fread(puIndices, 1, (size_t)uIndexBytes, ptFile) != (size_t)uIndexBytes)
        goto cleanup;

    for (uint32_t i = 0; i < uIndices; ++i)
    {
        if (puIndices[i] >= uVertices)
            goto cleanup;
    }
    bOK = true;

    cleanup:
        if (ptFile)
            fclose(ptFile);
        if (!bOK)
        {
            if (ptVertices) PL_FREE(ptVertices);
            if (puIndices) PL_FREE(puIndices);
            return false;
        }
        *pptVertices = ptVertices;
        *ppuIndices = puIndices;
        *puVertexCount = uVertices;
        *puIndexCount = uIndices;
        return true;
}

static void
pl__handle_residency(plTerrain* ptTerrain)
{
    const uint64_t uCurrentFrame = gptIOI->get_io()->ulFrameCount;

    plTerrainResidencyNode* ptCurrentRequest = ptTerrain->tRequestQueue.ptNext;

    if(ptCurrentRequest)
    {

        plTerrainChunk* ptChunk = ptCurrentRequest->ptChunk;

        plTerrainVertex* ptVertices = NULL;
        uint32_t* ptIndices = NULL;
        uint32_t uVertexCount = 0;
        uint32_t uIndexCount = 0;

        if (ptChunk->uFileID >= pl_sb_size(ptTerrain->sbtChunkFiles) || !pl__read_chunk_mesh(ptTerrain->sbtChunkFiles[ptChunk->uFileID].tFile.acFile,
                ptChunk->szFileLocation, ptTerrain->tVertexBufferManager.uSize, ptTerrain->tIndexBufferManager.uSize,
                &ptVertices, &ptIndices, &uVertexCount, &uIndexCount))
        {
            pl__remove_from_residency_queue(ptTerrain, ptChunk);
            /* Log this chunk/path; production code should throttle retries. */
            return;
        }

        // bytes needed for this chunk
        const uint64_t idx_bytes = (uint64_t)uIndexCount * sizeof(uint32_t);
        const uint64_t vtx_bytes = (uint64_t)uVertexCount * sizeof(plTerrainVertex);

        const uint64_t uVertexStageOffset = idx_bytes;
        const uint64_t uIndexStageOffset = 0;

        // Try once
        plFreeListNode* idx_hole = gptFreeList->get_node(&ptTerrain->tIndexBufferManager, idx_bytes);
        plFreeListNode* vtx_hole = gptFreeList->get_node(&ptTerrain->tVertexBufferManager, vtx_bytes);

        if (!idx_hole || !vtx_hole)
        {
            if (idx_hole) gptFreeList->return_node(&ptTerrain->tIndexBufferManager, idx_hole);
            if (vtx_hole) gptFreeList->return_node(&ptTerrain->tVertexBufferManager, vtx_hole);

            // Free enough for BOTH pools and try again
            pl__free_chunk_until(ptTerrain, idx_bytes, vtx_bytes);

            idx_hole = gptFreeList->get_node(&ptTerrain->tIndexBufferManager, idx_bytes);
            vtx_hole = gptFreeList->get_node(&ptTerrain->tVertexBufferManager, vtx_bytes);
        }

        if (!idx_hole || !vtx_hole)
        {
            if (idx_hole) gptFreeList->return_node(&ptTerrain->tIndexBufferManager, idx_hole);
            if (vtx_hole) gptFreeList->return_node(&ptTerrain->tVertexBufferManager, vtx_hole);

            PL_FREE(ptVertices);
            PL_FREE(ptIndices);
            // fclose(ptDataFile);
            PL_LOG_WARN_API(gptLog, gptData->uLogChannelTerrain, "No Memory (post-eviction)");
            pl__defer_residency_head(ptTerrain);
            return;
        }

        ptChunk->ptIndexHole = idx_hole;
        ptChunk->ptVertexHole = vtx_hole;
        ptChunk->uIndexCount  = uIndexCount;

        // destination offsets
        const uint64_t uIndexFinalOffset = ptChunk->ptIndexHole->uOffset;
        const uint64_t uVertexFinalOffset = ptChunk->ptVertexHole->uOffset;

        // copy memory to mapped staging buffer
        gptStage->stage_buffer_upload(ptTerrain->tIndexBuffer, uIndexFinalOffset, ptIndices, idx_bytes);
        gptStage->stage_buffer_upload(ptTerrain->tVertexBuffer, uVertexFinalOffset, ptVertices, vtx_bytes);
        gptStage->flush();

        PL_FREE(ptVertices);
        PL_FREE(ptIndices);

        pl__remove_from_residency_queue(ptTerrain, ptCurrentRequest->ptChunk);
        pl__touch_chunk(ptTerrain, ptChunk);

        PL_LOG_DEBUG_API(gptLog, gptData->uLogChannelTerrain, "Made chunk resident");
    }
}

static inline void
pl__lru_unlink(plTerrain* P, plTerrainChunk* c)
{
    if (!c->bInReplacementList) return;
    if (c->ptPrev) c->ptPrev->ptNext = c->ptNext;
    else           P->tReplacementQueue.ptNext = c->ptNext;
    if (c->ptNext) c->ptNext->ptPrev = c->ptPrev;
    c->ptPrev = c->ptNext = NULL;
    c->bInReplacementList = false;
}

static inline void
pl__lru_push_front(plTerrain* P, plTerrainChunk* c)
{
    // Head insert
    c->ptPrev = NULL;
    c->ptNext = P->tReplacementQueue.ptNext;
    if (c->ptNext) c->ptNext->ptPrev = c;
    P->tReplacementQueue.ptNext = c;
    c->bInReplacementList = true;
}


static void
pl__touch_chunk(plTerrain* P, plTerrainChunk* c)
{
    if (!c) return;
    c->uLastFrameUsed = gptIOI->get_io()->ulFrameCount;

    pl__lru_unlink(P, c);
    pl__lru_push_front(P, c);
}

static void
pl__remove_from_replacement_queue(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    pl__lru_unlink(ptTerrain, ptChunk);
}

static void
pl__request_residency(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    plTerrainResidencyNode* ptNode = NULL;
    plTerrainResidencyNode* ptTail = NULL;
    uint32_t uSlot = 0;
    uint64_t uFrame = 0;

    if (!ptChunk)
        return;

    if (ptChunk->ptIndexHole && ptChunk->ptVertexHole && ptChunk->uIndexCount != 0)
        return;

    uFrame = gptIOI->get_io()->ulFrameCount;
    for(ptNode = ptTerrain->tRequestQueue.ptNext; ptNode; ptNode = ptNode->ptNext)
    {
        if (ptNode->ptChunk == ptChunk)
        {
            ptNode->uFrameRequested = uFrame;
            return;
        }
        ptTail = ptNode;
    }

    // Keep admitted work. Traversal retries unadmitted work next frame.
    if (pl_sb_size(ptTerrain->sbuFreeRequests) == 0)
        return;

    uSlot = pl_sb_pop(ptTerrain->sbuFreeRequests);
    ptNode = &ptTerrain->atRequests[uSlot];
    ptNode->ptChunk = ptChunk;
    ptNode->uFrameRequested = uFrame;
    ptNode->ptPrev = ptTail;
    ptNode->ptNext = NULL;
    if (ptTail)
        ptTail->ptNext = ptNode;
    else
        ptTerrain->tRequestQueue.ptNext = ptNode;
}

static void
pl__render_chunk(plScene* ptScene, plTerrain* ptTerrain, const plCamera* ptCamera, plCommandBuffer* ptCmdBuffer, plTerrainChunk* ptChunk, plTerrainChunkFile* ptFile, uint32_t uGlobalIndex, float fViewportWidth)
{
    PL_ASSERT(ptChunk != NULL);

    plAABB tAABB = {
        .tMin = ptChunk->tMinBound,
        .tMax = ptChunk->tMaxBound
    };

    plFrustum tFrustum = {0};
    if(ptCamera->eProjectionType == PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
        pl__camera_build_perspective_frustum(ptCamera, &tFrustum);
    else
        pl__camera_build_orthographic_frustum(ptCamera, &tFrustum);

    if(!gptGjk->pen(pl_gjk_support_aabb, &tAABB, pl_gjk_support_frustum, &tFrustum, NULL))
        return;

    plVec3 tClosestPoint = gptCollision->point_closest_point_aabb(ptCamera->tPositionF, tAABB);
    float fDistance = fabsf(pl_length_vec3(pl_sub_vec3(tClosestPoint, ptCamera->tPositionF)));

    pl__request_residency(ptTerrain, ptChunk);

    if(ptChunk->ptIndexHole == NULL)
        return;
    
    float fHorizontalFieldOfView = 2.0f * atanf(tanf(0.5f * ptCamera->fYFov) * ptCamera->fAspectRatio);

    float fK = fViewportWidth / (2.0f * tanf(0.5f * fHorizontalFieldOfView));

    float fGeometricError = ptFile->fMaxBaseError * (float)ptChunk->uLevel;
    float fRho = fGeometricError * fK / fDistance;

    plTerrainComponent* ptTerrainComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tTerrainComponentType, ptScene->tTerrain);
    plTerrainAsset* ptAsset = gptAsset->get_data(ptTerrain->tAsset);

    float tauSubdivide = ptTerrainComp->fTau;
    bool bChildrenResident = pl__all_children_resident(ptChunk);

    static plVec3 atColors[] = {
        { 0.1f, 0.0f, 0.0f},
        { 0.0f, 0.1f, 0.0f},
        { 0.0f, 0.0f, 0.1f},
        { 0.1f, 0.1f, 0.0f},
        { 0.1f, 0.0f, 0.1f},
        { 0.0f, 0.1f, 0.1f},
        { 0.1f, 0.1f, 0.1f},
        { 0.4f, 0.1f, 0.1f},
        { 0.4f, 0.0f, 0.0f},
        { 0.0f, 0.4f, 0.0f},
        { 0.0f, 0.0f, 0.4f},
        { 0.4f, 0.4f, 0.0f},
        { 0.4f, 0.0f, 0.4f},
        { 0.0f, 0.4f, 0.4f},
        { 0.4f, 0.4f, 0.4f},
        { 0.3f, 0.4f, 0.4f}
    };
    uint32_t uDebugColorSlot = 0;

    if(ptTerrainComp->tFlags & PL_TERRAIN_FLAGS_SHOW_TILES)
    {
        uint32_t uDepth = ptFile->iTreeDepth - 1 - ptChunk->uLevel;
        uint32_t uGridSize = 1u << uDepth;

        uint32_t uX = (uint32_t)roundf(ptChunk->fX * (float)uGridSize);
        uint32_t uY = (uint32_t)roundf(ptChunk->fY * (float)uGridSize);

        uDebugColorSlot = (uX * 3u + uY * 5u + ptChunk->uLevel * 7u) % 16u;
    }

    // Decide refinement using hysteresis
    if(!bChildrenResident || fRho <= tauSubdivide)
    {
        // Draw parent
        plDevice* ptDevice = gptData->ptDevice;
        plDynamicBinding tDynamicBinding =  pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynTerrainData));
        
        plGpuDynTerrainData tDynamic = {0};
        tDynamic.iLevel          = (int)ptChunk->uLevel;
        if(ptTerrainComp->tFlags & PL_TERRAIN_FLAGS_SHOW_LEVELS)
        {
            tDynamic.tFlags |= PL_TERRAIN_SHADER_FLAGS_SHOW_DEBUG_COLOR;
            tDynamic.tDebugColor.rgb = atColors[ptChunk->uLevel % 16];
        }

        if(ptTerrainComp->tFlags & PL_TERRAIN_FLAGS_SHOW_TILES)
        {
            tDynamic.tFlags |= PL_TERRAIN_SHADER_FLAGS_SHOW_DEBUG_COLOR;
            tDynamic.tDebugColor.rgb = atColors[uDebugColorSlot];
        }

        tDynamic.tUVInfo.xy       = ptChunk->tUVScale;
        tDynamic.tUVInfo.zw       = ptChunk->tUVOffset;
        tDynamic.iPointLightCount = pl_sb_size(ptScene->sbtPointLights);
        tDynamic.iSpotLightCount = pl_sb_size(ptScene->sbtSpotLights);
        tDynamic.iDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLights);
        tDynamic.iProbeCount = pl_sb_size(ptScene->sbtProbeData);
        tDynamic.uGlobalIndex = uGlobalIndex;
        tDynamic.fSlopeStart = ptTerrainComp->fSlopeStart;
        tDynamic.fSlopeEnd = ptTerrainComp->fSlopeEnd;
        
        uint32_t uZoneCount = ptAsset->uElevationZoneCount;
        size_t szZoneCapacity = sizeof(tDynamic.atElevationZones) / sizeof(tDynamic.atElevationZones[0]);

        if (!ptAsset->atElevationZones)
            uZoneCount = 0;
        if ((size_t)uZoneCount > szZoneCapacity)
            uZoneCount = (uint32_t)szZoneCapacity;

        for(uint32_t i = 0; i < uZoneCount; i++)
        {
            tDynamic.atElevationZones[i].tElevation.x = ptAsset->atElevationZones[i].fMinElevation;
            tDynamic.atElevationZones[i].tElevation.y = ptAsset->atElevationZones[i].fMaxElevation;
            tDynamic.atElevationZones[i].tElevation.z = ptAsset->atElevationZones[i].fBlendSize;

            plMaterial* ptFlatMaterial = gptAsset->get_data(ptAsset->atElevationZones[i].tFlatMaterial);
            plMaterial* ptSteepMaterial = gptAsset->get_data(ptAsset->atElevationZones[i].tSteepMaterial);

            tDynamic.atElevationZones[i].tFlatMaterial.tBaseColor = ptFlatMaterial->tBaseColor;
            tDynamic.atElevationZones[i].tSteepMaterial.tBaseColor = ptSteepMaterial->tBaseColor;
        }

        plGpuDynTerrainData* ptDynamic = (plGpuDynTerrainData*)tDynamicBinding.pcData;
        memcpy(ptDynamic, &tDynamic, sizeof(plGpuDynTerrainData));
        gptGfx->bind_graphics_bind_groups(ptCmdBuffer, ptScene->tTerrainShader, 0, 0, NULL, 1, &tDynamicBinding);

        PL_ASSERT(ptChunk->ptVertexHole->uOffset % sizeof(plTerrainVertex) == 0);

        const plDrawIndex tDraw = {
            .uInstanceCount = 1,
            .uIndexCount    = ptChunk->uIndexCount,
            .uVertexStart   = (uint32_t)(ptChunk->ptVertexHole->uOffset / sizeof(plTerrainVertex)),
            .uIndexStart    = (uint32_t)(ptChunk->ptIndexHole->uOffset / sizeof(uint32_t)),
            .tIndexBuffer   = ptTerrain->tIndexBuffer
        };

        gptGfx->draw_indexed(ptCmdBuffer, 1, &tDraw);
        *gptData->pdDrawCalls += 1;

        pl__touch_chunk(ptTerrain, ptChunk);

        // --- Hysteresis refinement logic ---
        if(fRho > tauSubdivide)
        {
            // Need children soon – schedule them
            for(uint32_t i = 0; i < 4; i++)
            {
                if(ptChunk->aptChildren[i])
                    pl__request_residency(ptTerrain, ptChunk->aptChildren[i]);
            }
        }
    }
    else
    {
        // Descend into children
        for(uint32_t i = 0; i < 4; i++)
            pl__render_chunk(ptScene, ptTerrain, ptCamera, ptCmdBuffer, ptChunk->aptChildren[i], ptFile, uGlobalIndex, fViewportWidth);
    }
}

static void
pl__render_chunk_shadow(plScene* ptScene, plTerrain* ptTerrain, const plCamera* ptCamera,
    plCommandBuffer* ptCmdBuffer, plTerrainChunk* ptChunk, plTerrainChunkFile* ptFile, uint32_t uGlobalIndex,
    float fShadowViewportWidth, float fShadowViewportHeight)
{
    PL_ASSERT(ptChunk != NULL);

    plAABB tAABB = {
        .tMin = ptChunk->tMinBound,
        .tMax = ptChunk->tMaxBound
    };

    plFrustum tFrustum = {0};
    if(ptCamera->eProjectionType == PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
        pl__camera_build_perspective_frustum(ptCamera, &tFrustum);
    else
        pl__camera_build_orthographic_frustum(ptCamera, &tFrustum);

    if(!gptGjk->pen(pl_gjk_support_aabb, &tAABB, pl_gjk_support_frustum, &tFrustum, NULL))
        return;

    pl__request_residency(ptTerrain, ptChunk);

    if(ptChunk->ptIndexHole == NULL)
        return;

    plTerrainComponent* ptTerrainComp = gptEcs->get_component(ptScene->ptComponentLibrary, gptData->tTerrainComponentType, ptScene->tTerrain);

    // Geometric error is measured in world units.
    float fGeometricError = ptFile->fMaxBaseError * (float)ptChunk->uLevel;

    // Orthographic projection has constant screen-space scale.
    const float fProjectionScaleX = fabsf(ptCamera->tProjMat.col[0].x);
    const float fProjectionScaleY = fabsf(ptCamera->tProjMat.col[1].y);
    const float fPixelsPerWorldUnitX = 0.5f * fShadowViewportWidth * fProjectionScaleX;
    const float fPixelsPerWorldUnitY = 0.5f * fShadowViewportHeight * fProjectionScaleY;

    // Conservative choice: refine according to the axis with the
    // greatest pixel density.
    const float fPixelsPerWorldUnit = pl_max(fPixelsPerWorldUnitX, fPixelsPerWorldUnitY);
    const float fRho = fGeometricError * fPixelsPerWorldUnit;
    const float fTauSubdivide = ptTerrainComp->fTau;
    const bool bChildrenResident = pl__all_children_resident(ptChunk);

    if(!bChildrenResident || fRho <= fTauSubdivide)
    {
        const plDrawIndex tDraw = {
            .uIndexCount    = ptChunk->uIndexCount,
            .uVertexStart   = (uint32_t)(ptChunk->ptVertexHole->uOffset / sizeof(plTerrainVertex)),
            .uIndexStart    = (uint32_t)(ptChunk->ptIndexHole->uOffset / sizeof(uint32_t)),
            .tIndexBuffer   = ptTerrain->tIndexBuffer,
            .uInstanceCount = 1
        };

        gptGfx->draw_indexed(ptCmdBuffer, 1, &tDraw);
        *gptData->pdDrawCalls += 1;

        pl__touch_chunk(ptTerrain, ptChunk);

        if(fRho > fTauSubdivide)
        {
            for(uint32_t i = 0; i < 4; i++)
            {
                if(ptChunk->aptChildren[i])
                    pl__request_residency(ptTerrain, ptChunk->aptChildren[i]);
            }
        }
    }
    else
    {
        for(uint32_t i = 0; i < 4; i++)
        {
            if(ptChunk->aptChildren[i])
                pl__render_chunk_shadow(ptScene, ptTerrain, ptCamera, ptCmdBuffer, ptChunk->aptChildren[i], ptFile,
                    uGlobalIndex, fShadowViewportWidth, fShadowViewportHeight);
        }
    }
}
