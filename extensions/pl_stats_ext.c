/*
   pl_stats_ext.c
*/

/*
Index of this file:
// [SECTION] notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global context
// [SECTION] internal api
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] notes
//-----------------------------------------------------------------------------

/*
    The pointers provided by counters should remain valid forever, so
    allocations are handled in blocks.
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STATS_BLOCK_COUNT
    #define PL_STATS_BLOCK_COUNT 256
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <string.h>
#include "pl.h"
#include "pl_stats_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plStatsSource
{
    const char* pcName;
    double      dFrameValue;
    double*     dFrameValues;
} plStatsSource;

typedef struct _plStatsSourceBlock plStatsSourceBlock;
typedef struct _plStatsSourceBlock
{
    plStatsSource       atSources[PL_STATS_BLOCK_COUNT];
    plStatsSourceBlock* ptNextBlock;
} plStatsSourceBlock;

typedef struct _plStatsContext
{
    uint32_t             uBlockCount;
    uint32_t             uNextSource;
    plStatsSourceBlock   tInitialBlock;
    plStatsSourceBlock** sbtBlocks;
    plHashMap64          tHashmap;
    uint64_t             uCurrentFrame;
    const char**         sbtNames;
    uint32_t             uMaxFrames;
} plStatsContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plStatsContext* gptStatsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static double*      pl__get_counter     (char const* pcName);
static void         pl__new_frame       (void);
static double**     pl__get_counter_data(char const* pcName);
static const char** pl__get_names       (uint32_t* puCount);
static uint32_t     pl__get_max_frames  (void);
static void         pl__set_max_frames  (uint32_t);

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static double*
pl__get_counter(char const* pcName)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName, 0);

    uint64_t ulIndex = UINT64_MAX;
    if(!pl_hm_has_key_ex(&gptStatsCtx->tHashmap, ulHash, &ulIndex))
    {
        pl_sb_push(gptStatsCtx->sbtNames, pcName);
        ulIndex = pl_hm_get_free_index(&gptStatsCtx->tHashmap);
        if(ulIndex == PL_DS_HASH_INVALID)
        {
            ulIndex = gptStatsCtx->uNextSource++;
            
            // check if need to add another block
            if(gptStatsCtx->uNextSource >= PL_STATS_BLOCK_COUNT * gptStatsCtx->uBlockCount)
            {
                plStatsSourceBlock* ptLastBlock = &gptStatsCtx->tInitialBlock;
                while(ptLastBlock->ptNextBlock)
                    ptLastBlock = ptLastBlock->ptNextBlock;
                ptLastBlock->ptNextBlock = PL_ALLOC(sizeof(plStatsSourceBlock));
                memset(ptLastBlock->ptNextBlock, 0, sizeof(plStatsSourceBlock));
                gptStatsCtx->uBlockCount++;
                pl_sb_push(gptStatsCtx->sbtBlocks, ptLastBlock->ptNextBlock);
            }
        }

        if(pl_sb_size(gptStatsCtx->sbtBlocks) == 0)
            pl_sb_push(gptStatsCtx->sbtBlocks, &gptStatsCtx->tInitialBlock);

        pl_hm_insert(&gptStatsCtx->tHashmap, ulHash, ulIndex);
    }
    const uint64_t ulBlockIndex = (uint64_t)floor((double)ulIndex / (double)PL_STATS_BLOCK_COUNT);
    gptStatsCtx->sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].pcName = pcName;
    return &gptStatsCtx->sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].dFrameValue;
}

static void
pl__new_frame(void)
{
    const uint64_t ulFrameIndex = gptStatsCtx->uCurrentFrame % gptStatsCtx->uMaxFrames;
    gptStatsCtx->uCurrentFrame++;
    plStatsSourceBlock* ptLastBlock = &gptStatsCtx->tInitialBlock;
    uint32_t uSourcesRemaining = gptStatsCtx->uNextSource;
    while(ptLastBlock)
    {
        if(ptLastBlock->ptNextBlock)
        {
            for(uint32_t i = 0; i < PL_STATS_BLOCK_COUNT; i++)
            {
                if(ptLastBlock->atSources[i].dFrameValues)
                    ptLastBlock->atSources[i].dFrameValues[ulFrameIndex] = ptLastBlock->atSources[i].dFrameValue;
                ptLastBlock->atSources[i].dFrameValue = 0.0;
            }
        }
        else
        {
            for(uint32_t i = 0; i < gptStatsCtx->uNextSource % PL_STATS_BLOCK_COUNT; i++)
            {
                if(ptLastBlock->atSources[i].dFrameValues)
                    ptLastBlock->atSources[i].dFrameValues[ulFrameIndex] = ptLastBlock->atSources[i].dFrameValue;
                ptLastBlock->atSources[i].dFrameValue = 0.0;
            }  
        }
        ptLastBlock = ptLastBlock->ptNextBlock;
    }     
}

static const char**
pl__get_names(uint32_t* puCount)
{
    if(puCount)
        *puCount = pl_sb_size(gptStatsCtx->sbtNames);
    return gptStatsCtx->sbtNames;
}

static uint32_t
pl__get_max_frames(void)
{
    return gptStatsCtx->uMaxFrames;
}

static void
pl__set_max_frames(uint32_t uMaxFrames)
{
    gptStatsCtx->uMaxFrames = uMaxFrames;
}

static double**
pl__get_counter_data(char const* pcName)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName, 0);
    uint64_t ulIndex = UINT64_MAX;
    if(!pl_hm_has_key_ex(&gptStatsCtx->tHashmap, ulHash, &ulIndex))
    {
        pl_sb_push(gptStatsCtx->sbtNames, pcName);
        ulIndex = pl_hm_get_free_index(&gptStatsCtx->tHashmap);
        if(ulIndex == PL_DS_HASH_INVALID)
        {
            ulIndex = gptStatsCtx->uNextSource++;
            
            // check if we need to add another block
            if(gptStatsCtx->uNextSource >= PL_STATS_BLOCK_COUNT * gptStatsCtx->uBlockCount)
            {
                plStatsSourceBlock* ptLastBlock = &gptStatsCtx->tInitialBlock;
                while(ptLastBlock->ptNextBlock)
                    ptLastBlock = ptLastBlock->ptNextBlock;
                ptLastBlock->ptNextBlock = PL_ALLOC(sizeof(plStatsSourceBlock));
                memset(ptLastBlock->ptNextBlock, 0, sizeof(plStatsSourceBlock));
                gptStatsCtx->uBlockCount++;
                pl_sb_push(gptStatsCtx->sbtBlocks, ptLastBlock->ptNextBlock);
            }
        }

        if(pl_sb_size(gptStatsCtx->sbtBlocks) == 0)
            pl_sb_push(gptStatsCtx->sbtBlocks, &gptStatsCtx->tInitialBlock);

        pl_hm_insert(&gptStatsCtx->tHashmap, ulHash, ulIndex);
    }
    const uint64_t ulBlockIndex = (uint64_t)floor((double)ulIndex / (double)PL_STATS_BLOCK_COUNT);
    gptStatsCtx->sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].pcName = pcName;
    return &gptStatsCtx->sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].dFrameValues;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_stats_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plStatsI tApi = {
        .get_counter      = pl__get_counter,
        .new_frame        = pl__new_frame,
        .get_counter_data = pl__get_counter_data,
        .get_names        = pl__get_names,
        .set_max_frames   = pl__set_max_frames,
        .get_max_frames   = pl__get_max_frames
    };
    pl_set_api(ptApiRegistry, plStatsI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptStatsCtx = ptDataRegistry->get_data("plStatsContext");
    }
    else // first load
    {
        static plStatsContext gtStatsCtx = {
            .uBlockCount = 1,
            .uMaxFrames  = 120
        };
        gptStatsCtx = &gtStatsCtx;
        ptDataRegistry->set_data("plStatsContext", gptStatsCtx);
    }
}

PL_EXPORT void
pl_unload_stats_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plStatsI* ptApi = pl_get_api_latest(ptApiRegistry, plStatsI);
    ptApiRegistry->remove_api(ptApi);

    pl_sb_free(gptStatsCtx->sbtBlocks);
    pl_sb_free(gptStatsCtx->sbtNames);
    pl_hm_free(&gptStatsCtx->tHashmap);
}
