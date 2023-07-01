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
// [SECTION] public api implementation
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
#include "pilotlight.h"
#include "pl_stats_ext.h"
#include "pl_ds.h"
#include "pl_memory.h"

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
    plHashMap            tHashmap;
    uint64_t             uCurrentFrame;
    const char**         sbtNames;
} plStatsContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plStatsContext gtStatsContext = {1, 0};

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static double*      pl__get_counter     (char const* pcName);
static void         pl__new_frame       (void);
static double**     pl__get_counter_data(char const* pcName);
static const char** pl__get_names       (uint32_t* puCount);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plStatsApiI*
pl_load_stats_api(void)
{
    static const plStatsApiI tApi = {
        .get_counter      = pl__get_counter,
        .new_frame        = pl__new_frame,
        .get_counter_data = pl__get_counter_data,
        .get_names        = pl__get_names
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static double*
pl__get_counter(char const* pcName)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName);

    const bool bDataExists = pl_hm_has_key(&gtStatsContext.tHashmap, ulHash);

    uint64_t ulIndex = UINT64_MAX;

    if(bDataExists)
        ulIndex = pl_hm_lookup(&gtStatsContext.tHashmap, ulHash);
    else
    {
        pl_sb_push(gtStatsContext.sbtNames, pcName);
        ulIndex = pl_hm_get_free_index(&gtStatsContext.tHashmap);
        if(ulIndex == UINT64_MAX)
        {
            ulIndex = gtStatsContext.uNextSource++;
            
            // check if need to add another block
            if(gtStatsContext.uNextSource >= PL_STATS_BLOCK_COUNT * gtStatsContext.uBlockCount)
            {
                plStatsSourceBlock* ptLastBlock = &gtStatsContext.tInitialBlock;
                while(ptLastBlock->ptNextBlock)
                    ptLastBlock = ptLastBlock->ptNextBlock;
                ptLastBlock->ptNextBlock = PL_ALLOC(sizeof(plStatsSourceBlock));
                memset(ptLastBlock->ptNextBlock, 0, sizeof(plStatsSourceBlock));
                gtStatsContext.uBlockCount++;
                pl_sb_push(gtStatsContext.sbtBlocks, ptLastBlock->ptNextBlock);
            }
        }

        if(pl_sb_size(gtStatsContext.sbtBlocks) == 0)
            pl_sb_push(gtStatsContext.sbtBlocks, &gtStatsContext.tInitialBlock);

        pl_hm_insert(&gtStatsContext.tHashmap, ulHash, ulIndex);
    }
    const uint64_t ulBlockIndex = (uint64_t)floor((double)ulIndex / (double)PL_STATS_BLOCK_COUNT);
    gtStatsContext.sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].pcName = pcName;
    return &gtStatsContext.sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].dFrameValue;
}

static void
pl__new_frame(void)
{
    const uint64_t ulFrameIndex = gtStatsContext.uCurrentFrame % PL_STATS_MAX_FRAMES;
    gtStatsContext.uCurrentFrame++;
    plStatsSourceBlock* ptLastBlock = &gtStatsContext.tInitialBlock;
    uint32_t uSourcesRemaining = gtStatsContext.uNextSource;
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
            for(uint32_t i = 0; i < gtStatsContext.uNextSource % PL_STATS_BLOCK_COUNT; i++)
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
        *puCount = pl_sb_size(gtStatsContext.sbtNames);
    return gtStatsContext.sbtNames;
}

static double**
pl__get_counter_data(char const* pcName)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName);

    const bool bDataExists = pl_hm_has_key(&gtStatsContext.tHashmap, ulHash);

    uint64_t ulIndex = UINT64_MAX;

    if(bDataExists)
        ulIndex = pl_hm_lookup(&gtStatsContext.tHashmap, ulHash);
    else
    {
        pl_sb_push(gtStatsContext.sbtNames, pcName);
        ulIndex = pl_hm_get_free_index(&gtStatsContext.tHashmap);
        if(ulIndex == UINT64_MAX)
        {
            ulIndex = gtStatsContext.uNextSource++;
            
            // check if we need to add another block
            if(gtStatsContext.uNextSource >= PL_STATS_BLOCK_COUNT * gtStatsContext.uBlockCount)
            {
                plStatsSourceBlock* ptLastBlock = &gtStatsContext.tInitialBlock;
                while(ptLastBlock->ptNextBlock)
                    ptLastBlock = ptLastBlock->ptNextBlock;
                ptLastBlock->ptNextBlock = PL_ALLOC(sizeof(plStatsSourceBlock));
                memset(ptLastBlock->ptNextBlock, 0, sizeof(plStatsSourceBlock));
                gtStatsContext.uBlockCount++;
                pl_sb_push(gtStatsContext.sbtBlocks, ptLastBlock->ptNextBlock);
            }
        }

        if(pl_sb_size(gtStatsContext.sbtBlocks) == 0)
            pl_sb_push(gtStatsContext.sbtBlocks, &gtStatsContext.tInitialBlock);

        pl_hm_insert(&gtStatsContext.tHashmap, ulHash, ulIndex);
    }
    const uint64_t ulBlockIndex = (uint64_t)floor((double)ulIndex / (double)PL_STATS_BLOCK_COUNT);
    gtStatsContext.sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].pcName = pcName;
    return &gtStatsContext.sbtBlocks[ulBlockIndex]->atSources[ulIndex % PL_STATS_BLOCK_COUNT].dFrameValues;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_stats_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    const plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));

    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_STATS), pl_load_stats_api());
    else
        ptApiRegistry->add(PL_API_STATS, pl_load_stats_api());
}

PL_EXPORT void
pl_unload_stats_ext(plApiRegistryApiI* ptApiRegistry)
{
    pl_sb_free(gtStatsContext.sbtBlocks);
    pl_sb_free(gtStatsContext.sbtNames);
}
