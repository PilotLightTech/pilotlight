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
    plRenderAttachmentInfo tRenderPassLayoutHandle;
    plTerrainRuntimeOptions tRuntimeOptions;
    plChunkFileData* sbtChunkFiles;
    plTerrainProcessInfo tInfo;
    plVec2           tTopLeftGlobal;
    plVec2           tBottomRightGlobal;
    uint32_t                 uTileCount;
    plTerrainProcessTileInfo* atTiles;
    
    plTerrainResidencyNode tRequestQueue;
    plTerrainResidencyNode atRequests[PL_REQUEST_QUEUE_SIZE];
    uint32_t*             sbuFreeRequests;

    plTerrainChunk tReplacementQueue;

    plBufferHandle tIndexBuffer;
    plFreeList tIndexBufferManager;
    
    plBufferHandle tVertexBuffer;
    plFreeList tVertexBufferManager;
} plTerrain;

//-----------------------------------------------------------------------------
// [SECTION] internal helpers (rendering)
//-----------------------------------------------------------------------------

// rendering
static void pl__handle_residency (plTerrain*);
static void pl__request_residency(plTerrain*, plTerrainChunk*);
static void pl__touch_chunk(plTerrain*, plTerrainChunk*);
static void pl__make_unresident  (plTerrain*, plTerrainChunk*);
static bool pl__terrain_load(plTerrain* ptTerrain, plTerrainProcessInfo* ptInfo);
void pl__remove_from_replacement_queue(plTerrain* ptTerrain, plTerrainChunk* ptChunk);

static void pl__render_chunk(plScene*, plTerrain*, const plCamera*, plCommandBuffer*, plTerrainChunk*, plTerrainChunkFile*, const plMat4* ptMVP, uint32_t);
static void pl__render_chunk_shadow(plScene*, plTerrain*, const plCamera*, plCommandBuffer*, plTerrainChunk*, plTerrainChunkFile*, const plMat4* ptMVP, uint32_t);

static void pl__free_chunk_until(plTerrain* P, uint64_t idx_bytes_needed, uint64_t vtx_bytes_needed);


static inline bool pl__is_leaf_resident(const plTerrainChunk* c)
{
    if (!c->aptChildren[0]) return true; // no children in tree
    return !(c->aptChildren[0]->ptIndexHole ||
             c->aptChildren[1]->ptIndexHole ||
             c->aptChildren[2]->ptIndexHole ||
             c->aptChildren[3]->ptIndexHole);
}



//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plTerrain*
pl_renderer_terrain_create(plCommandBuffer* ptCmdBuffer, plTerrainProcessInfo* ptInfo)
{
    plTerrain* ptTerrain = PL_ALLOC(sizeof(plTerrain));
    memset(ptTerrain, 0, sizeof(plTerrain));


    ptTerrain->tRenderPassLayoutHandle = gptData->tRenderPassLayout;
    ptTerrain->tInfo = *ptInfo;
    ptTerrain->tRuntimeOptions.fTau = 0.2f;

    ptTerrain->tRuntimeOptions.fSlopeStart = 0.0f;
    ptTerrain->tRuntimeOptions.fSlopeEnd = 0.45f;
    ptTerrain->tRuntimeOptions.atElevationZones[0].fMinElevation = -1000.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[0].fMaxElevation = 20.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[0].fBlendSize = 30.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[0].tFlatMaterial.tBaseColor = (plVec4){0.66f, 0.598f, 0.402f, 1.0f};
    ptTerrain->tRuntimeOptions.atElevationZones[0].tSteepMaterial.tBaseColor = (plVec4){0.20f, 0.18f, 0.16f, 1.0f};

    ptTerrain->tRuntimeOptions.fTerrainShadowConstantDepthBias = -100.0f;
    ptTerrain->tRuntimeOptions.fTerrainShadowSlopeDepthBias = -10.0f;

    ptTerrain->tRuntimeOptions.atElevationZones[1].fMinElevation = 20.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[1].fMaxElevation = 1000.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[1].fBlendSize = 40.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[1].tFlatMaterial.tBaseColor  = (plVec4){0.05f, 0.30f, 0.05f, 1.0f};
    ptTerrain->tRuntimeOptions.atElevationZones[1].tSteepMaterial.tBaseColor = (plVec4){0.08f, 0.07f, 0.06f, 1.0f};

    ptTerrain->tRuntimeOptions.atElevationZones[2].fMinElevation = 1000.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[2].fMaxElevation = 2000.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[2].fBlendSize = 60.0f;
    ptTerrain->tRuntimeOptions.atElevationZones[2].tFlatMaterial.tBaseColor = (plVec4){0.85f, 0.85f, 0.82f, 1.0f};
    ptTerrain->tRuntimeOptions.atElevationZones[2].tSteepMaterial.tBaseColor = (plVec4){0.45f, 0.45f, 0.43f, 1.0f};

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

    pl__terrain_load(ptTerrain, ptInfo);
    ptTerrain->atTiles = PL_ALLOC(sizeof(plTerrainProcessTileInfo) * ptInfo->uTileCount);
    memset(ptTerrain->atTiles, 0, sizeof(plTerrainProcessTileInfo) * ptInfo->uTileCount);
    memcpy(ptTerrain->atTiles, ptInfo->atTiles, sizeof(plTerrainProcessTileInfo) * ptInfo->uTileCount);
    ptTerrain->uTileCount = ptInfo->uTileCount;

    for(uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
        pl__request_residency(ptTerrain, &ptTerrain->sbtChunkFiles[i].tFile.atChunks[0]);
    return ptTerrain;
}

PL_API plTerrainRuntimeOptions*
pl_renderer_terrain_get_runtime_options(plTerrain* ptTerrain)
{
    return &ptTerrain->tRuntimeOptions;
}

void
pl_renderer_terrain_destroy(plTerrain* ptTerrain)
{
    plDevice* ptDevice = gptData->ptDevice;

    for(uint32_t i = 0; i < pl_sb_size(ptTerrain->sbtChunkFiles); i++)
    {
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

bool
pl__all_children_resident(plTerrainChunk* ptChunk)
{
    if(ptChunk->aptChildren[0] == NULL)
        return false;
    if(ptChunk->aptChildren[0]->ptIndexHole == NULL) return false;
    if(ptChunk->aptChildren[1]->ptIndexHole == NULL) return false;
    if(ptChunk->aptChildren[2]->ptIndexHole == NULL) return false;
    if(ptChunk->aptChildren[3]->ptIndexHole == NULL) return false;
    return true;
}

// Free from LRU tail until we have at least the requested bytes for BOTH pools.
// Never evict level 0, and never evict non-leaves (chunks with resident children).
static void
pl__free_chunk_until(plTerrain* P,
                     uint64_t idx_bytes_needed,
                     uint64_t vtx_bytes_needed)
{
    // Walk to tail (oldest / least-recently-used)
    plTerrainChunk* tail = P->tReplacementQueue.ptNext;
    if (!tail) {
        printf("Couldn't free chunks (replacement list empty)\n");
        return;
    }
    while (tail->ptNext) tail = tail->ptNext;

    uint64_t freed_idx = 0;
    uint64_t freed_vtx = 0;

    // Pass 1: strictly leaf, non-root, LRU → MRU until enough bytes
    for (plTerrainChunk* c = tail; c; c = c->ptPrev) {
        if (!c->ptIndexHole) continue;            // not resident -> skip
        if (c->uLevel == 0) continue;             // keep roots
        if (!pl__is_leaf_resident(c)) continue;   // don't drop nodes with resident children

        freed_idx += (uint64_t)c->uIndexCount * sizeof(uint32_t);
        freed_vtx += (uint64_t)c->ptVertexHole->uSize; // size was requested to freelist; safe to use

        pl__make_unresident(P, c);

        if (freed_idx >= idx_bytes_needed && freed_vtx >= vtx_bytes_needed)
            return; // sufficient
    }

    // Pass 2: allow evicting non-leaf (still avoid root). Prefer aged items.
    const uint64_t now = gptIOI->get_io()->ulFrameCount;
    for (plTerrainChunk* c = tail; c; c = c->ptPrev) {
        if (!c->ptIndexHole) continue;
        if (c->uLevel == 0) continue;
        if (now - c->uLastFrameUsed <= 30) continue;

        freed_idx += (uint64_t)c->uIndexCount * sizeof(uint32_t);
        freed_vtx += (uint64_t)c->ptVertexHole->uSize;
        pl__make_unresident(P, c);

        if (freed_idx >= idx_bytes_needed && freed_vtx >= vtx_bytes_needed)
            return; // ✅ sufficient
    }

    // Pass 3: final fallback — evict oldest non-root regardless of age/leaf.
    for (plTerrainChunk* c = tail; c; c = c->ptPrev) {
        if (!c->ptIndexHole) continue;
        if (c->uLevel == 0) continue;

        pl__make_unresident(P, c);
        return; // free at least one to make progress
    }

    // If we reached here, nothing could be evicted (only root is resident)
    printf("Eviction failed: only root or no resident chunks available\n");
}


void
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

bool
pl_chlod_load_chunk_file(plTerrain* ptTerrain, const char* pcPath)
{
    plChunkFileData tChunkFileData = {0};
    uint32_t uChunkFileID = pl_sb_size(ptTerrain->sbtChunkFiles);
    gptTerrain->load_chunk_file(pcPath, &tChunkFileData.tFile, uChunkFileID);

    for(uint32_t i = 0; i < tChunkFileData.tFile.uChunkCount; i++)
    {

        tChunkFileData.tFile.atChunks[i].uIndex = i;

        tChunkFileData.tFile.atChunks[i].tUVScale.x = 1.0f;
        tChunkFileData.tFile.atChunks[i].tUVScale.y = 1.0f;
    }
    pl_sb_push(ptTerrain->sbtChunkFiles, tChunkFileData);
    return true;
}

static void
pl__make_unresident(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    pl__remove_from_residency_queue(ptTerrain, ptChunk);
    pl__remove_from_replacement_queue(ptTerrain, ptChunk);
    if(ptChunk->ptIndexHole && ptChunk->uIndexCount > 0)
    {
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

static void
pl__handle_residency(plTerrain* ptTerrain)
{
    const uint64_t uCurrentFrame = gptIOI->get_io()->ulFrameCount;

    plTerrainResidencyNode* ptCurrentRequest = ptTerrain->tRequestQueue.ptNext;
    // ptCurrentRequest->uFrameRequested

    if(ptCurrentRequest)
    {

        plTerrainChunk* ptChunk = ptCurrentRequest->ptChunk;

        FILE* ptDataFile = fopen(ptTerrain->sbtChunkFiles[ptChunk->uFileID].tFile.acFile, "rb");
        fseek(ptDataFile, (long)ptChunk->szFileLocation, SEEK_SET);

        fseek(ptDataFile, sizeof(plVec3) * 2 + sizeof(int) * 4, SEEK_CUR);

        uint32_t uVertexCount = 0;
        fread(&uVertexCount, 1, sizeof(uint32_t), ptDataFile);

        plTerrainVertex* ptVertices = PL_ALLOC(uVertexCount * sizeof(plTerrainVertex));
        fread(ptVertices, 1, sizeof(plTerrainVertex) * uVertexCount, ptDataFile);

        uint32_t uIndexCount = 0;
        fread(&uIndexCount, 1, sizeof(uint32_t), ptDataFile);
        
        uint32_t* ptIndices = PL_ALLOC(uIndexCount * sizeof(uint32_t));
        fread(ptIndices, 1, sizeof(uint32_t) * uIndexCount, ptDataFile);

        
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
            fclose(ptDataFile);
            printf("No Memory (post-eviction)\n");
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

        fclose(ptDataFile);

        pl__remove_from_residency_queue(ptTerrain, ptCurrentRequest->ptChunk);
        pl__touch_chunk(ptTerrain, ptChunk);
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

void
pl__remove_from_replacement_queue(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    pl__lru_unlink(ptTerrain, ptChunk);
}

static void
pl__request_residency(plTerrain* ptTerrain, plTerrainChunk* ptChunk)
{
    if(ptChunk == NULL)
        return;

    if(ptChunk->ptIndexHole == NULL)
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
            if(ptCurrentRequest->ptNext == NULL)
                ptLastRequest = ptCurrentRequest;
            ptCurrentRequest = ptCurrentRequest->ptNext;
        }

        if(ptExistingRequest)
        {

            // remove node
            if(ptExistingRequest->ptPrev)
                ptExistingRequest->ptPrev->ptNext = ptExistingRequest->ptNext;

            if(ptExistingRequest->ptNext)
                ptExistingRequest->ptNext->ptPrev = ptExistingRequest->ptPrev;    
        }
        else
        {
            if(pl_sb_size(ptTerrain->sbuFreeRequests) > 0)
            {
                uint32_t uNewIndex = pl_sb_pop(ptTerrain->sbuFreeRequests);
                ptExistingRequest = &ptTerrain->atRequests[uNewIndex];

            }
            else if(ptLastRequest)
            {
                if(ptLastRequest->ptPrev)
                    ptLastRequest->ptPrev->ptNext = NULL;
                ptExistingRequest = ptLastRequest; 
            }
            else
            {
                PL_ASSERT(false);
            }
        }

        // place node at beginning
        ptExistingRequest->ptPrev = NULL;
        if(ptExistingRequest != ptTerrain->tRequestQueue.ptNext)
            ptExistingRequest->ptNext = ptTerrain->tRequestQueue.ptNext;
        ptTerrain->tRequestQueue.ptNext = ptExistingRequest;
        if(ptExistingRequest->ptNext)
            ptExistingRequest->ptNext->ptPrev = ptExistingRequest;

        ptExistingRequest->uFrameRequested = gptIOI->get_io()->ulFrameCount;
        ptExistingRequest->ptChunk = ptChunk;
    }
}

static void
pl__render_chunk(plScene* ptScene, plTerrain* ptTerrain, const plCamera* ptCamera, plCommandBuffer* ptCmdBuffer, plTerrainChunk* ptChunk, plTerrainChunkFile* ptFile, const plMat4* ptMVP, uint32_t uGlobalIndex)
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
        pl_camera_build_orthographic_frustum(ptCamera, &tFrustum);

    if(!gptGjk->pen(pl_gjk_support_aabb, &tAABB, pl_gjk_support_frustum, &tFrustum, NULL))
        return;

    plVec3 tClosestPoint = gptCollision->point_closest_point_aabb(ptCamera->tPositionF, tAABB);
    float fDistance = fabsf(pl_length_vec3(pl_sub_vec3(tClosestPoint, ptCamera->tPositionF)));

    pl__request_residency(ptTerrain, ptChunk);

    if(ptChunk->ptIndexHole == NULL)
        return;
    
    float fViewportWidth = gptIOI->get_io()->tMainViewportSize.x;
    float fHorizontalFieldOfView = 2.0f * atanf(tanf(0.5f * ptCamera->fYFov) * ptCamera->fAspectRatio);

    float fK = fViewportWidth / (2.0f * tanf(0.5f * fHorizontalFieldOfView));

    float fGeometricError = ptFile->fMaxBaseError * (float)ptChunk->uLevel;
    float fRho = fGeometricError * fK / fDistance;


    float tauSubdivide = ptTerrain->tRuntimeOptions.fTau;
    float tauMerge     = tauSubdivide * 0.5f;

    bool bChildrenResident = pl__all_children_resident(ptChunk);

    // Decide refinement using hysteresis
    if(!bChildrenResident || fRho <= tauSubdivide)
    {
        // Draw parent
        plDevice* ptDevice = gptData->ptDevice;
        plDynamicBinding tDynamicBinding =  pl__allocate_dynamic_data(ptDevice, sizeof(plGpuDynTerrainData));
        plGpuDynTerrainData* ptDynamic = (plGpuDynTerrainData*)tDynamicBinding.pcData;

        ptDynamic->iLevel          = (int)ptChunk->uLevel;
        ptDynamic->tFlags          = ptTerrain->tRuntimeOptions.tFlags;
        ptDynamic->tUVInfo.xy       = ptChunk->tUVScale;
        ptDynamic->tUVInfo.zw       = ptChunk->tUVOffset;
        ptDynamic->iPointLightCount = pl_sb_size(ptScene->sbtPointLights);
        ptDynamic->iSpotLightCount = pl_sb_size(ptScene->sbtSpotLights);
        ptDynamic->iDirectionLightCount = pl_sb_size(ptScene->sbtDirectionLights);
        ptDynamic->iProbeCount = pl_sb_size(ptScene->sbtProbeData);
        ptDynamic->uGlobalIndex = uGlobalIndex;
        ptDynamic->fSlopeStart = ptTerrain->tRuntimeOptions.fSlopeStart;
        ptDynamic->fSlopeEnd = ptTerrain->tRuntimeOptions.fSlopeEnd;

        for(uint32_t i = 0; i < PL_MAX_TERRAIN_ELEVATION_ZONES; i++)
        {
            ptDynamic->atElevationZones[i].tElevation.x = ptTerrain->tRuntimeOptions.atElevationZones[i].fMinElevation;
            ptDynamic->atElevationZones[i].tElevation.y = ptTerrain->tRuntimeOptions.atElevationZones[i].fMaxElevation;
            ptDynamic->atElevationZones[i].tElevation.z = ptTerrain->tRuntimeOptions.atElevationZones[i].fBlendSize;
            ptDynamic->atElevationZones[i].tFlatMaterial.tBaseColor = ptTerrain->tRuntimeOptions.atElevationZones[i].tFlatMaterial.tBaseColor;
            ptDynamic->atElevationZones[i].tSteepMaterial.tBaseColor = ptTerrain->tRuntimeOptions.atElevationZones[i].tSteepMaterial.tBaseColor;
        }

        gptGfx->bind_graphics_bind_groups(
            ptCmdBuffer,
            ptScene->tTerrainShader,
            0, 0,
            NULL,
            1, &tDynamicBinding
        );

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
        else if(fRho < tauMerge)
        {
            // Clearly low detail – unload children
            pl__unload_children(ptTerrain, ptChunk);
        }
        // else: middle band → keep current state, prevent thrash
    }
    else
    {
        // Descend into children
        for(uint32_t i = 0; i < 4; i++)
            pl__render_chunk(ptScene, ptTerrain, ptCamera, ptCmdBuffer, ptChunk->aptChildren[i], ptFile, ptMVP, uGlobalIndex);
    }
}

static void
pl__render_chunk_shadow(plScene* ptScene, plTerrain* ptTerrain, const plCamera* ptCamera , plCommandBuffer* ptCmdBuffer, plTerrainChunk* ptChunk, plTerrainChunkFile* ptFile, const plMat4* ptMVP, uint32_t uGlobalIndex)
{
    PL_ASSERT(ptChunk != NULL);

    plAABB tAABB = {
        .tMin = ptChunk->tMinBound,
        .tMax = ptChunk->tMaxBound
    };

    // if(ptChunk->uLevel < 3)
    //     return;

    // if(!pl__renderer_sat_visibility_test(ptCamera, &tAABB))
    //     return;

    plVec3 tClosestPoint = gptCollision->point_closest_point_aabb(ptCamera->tPositionF, tAABB);
    float fDistance = fabsf(pl_length_vec3(pl_sub_vec3(tClosestPoint, ptCamera->tPositionF)));

    pl__request_residency(ptTerrain, ptChunk);

    if(ptChunk->ptIndexHole == NULL)
        return;
    
    float fViewportWidth = gptIOI->get_io()->tMainViewportSize.x;
    float fHorizontalFieldOfView = 2.0f * atanf(tanf(0.5f * ptCamera->fYFov) * ptCamera->fAspectRatio);

    float fK = fViewportWidth / (2.0f * tanf(0.5f * fHorizontalFieldOfView));

    float fGeometricError = ptFile->fMaxBaseError * (float)ptChunk->uLevel;
    float fRho = fGeometricError * fK / fDistance;


    float tauSubdivide = ptTerrain->tRuntimeOptions.fTau;
    float tauMerge     = tauSubdivide * 0.5f;

    bool bChildrenResident = pl__all_children_resident(ptChunk);

    // Decide refinement using hysteresis
    if(!bChildrenResident || fRho <= tauSubdivide || ptChunk->uLevel < 3)
    {
        const plDrawIndex tDraw = {
            .uIndexCount    = ptChunk->uIndexCount,
            .uVertexStart   = (uint32_t)(ptChunk->ptVertexHole->uOffset / sizeof(plTerrainVertex)),
            .uIndexStart    = (uint32_t)(ptChunk->ptIndexHole->uOffset / sizeof(uint32_t)),
            .tIndexBuffer   = ptTerrain->tIndexBuffer,
            .uInstanceCount = 1 // uCascadeCount
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
        else if(fRho < tauMerge)
        {
            // Clearly low detail – unload children
            pl__unload_children(ptTerrain, ptChunk);
        }
        // else: middle band → keep current state, prevent thrash
    }
    else
    {
        // Descend into children
        for(uint32_t i = 0; i < 4; i++)
            pl__render_chunk_shadow(ptScene, ptTerrain, ptCamera, ptCmdBuffer, ptChunk->aptChildren[i], ptFile, ptMVP, uGlobalIndex);
    }
}

static bool
pl__terrain_load(plTerrain* ptTerrain, plTerrainProcessInfo* ptInfo)
{
    {
        float fX = ptInfo->atTiles[0].tCenter.x;
        float fY = ptInfo->atTiles[0].tCenter.z;
        ptTerrain->tTopLeftGlobal.x = fX - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
        ptTerrain->tTopLeftGlobal.y = fY - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
    }

    {
        float fX = ptInfo->atTiles[ptInfo->uTileCount - 1].tCenter.x;
        float fY = ptInfo->atTiles[ptInfo->uTileCount - 1].tCenter.z;
        ptTerrain->tBottomRightGlobal.x = fX - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
        ptTerrain->tBottomRightGlobal.y = fY - 0.5f * (float)ptInfo->uSize * ptInfo->fMetersPerPixel;
    }

    for(uint32_t k = 0; k < ptInfo->uTileCount; k++)
    {
        uint32_t i = k % ptInfo->uHorizontalTiles;
        uint32_t j = (k - i) / ptInfo->uVerticalTiles;
        pl_chlod_load_chunk_file(ptTerrain, ptInfo->atTiles[k].acOutputFile);
    }
    return true;
}