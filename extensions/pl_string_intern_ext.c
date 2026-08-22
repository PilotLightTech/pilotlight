/*
   pl_string_intern_ext.c
     - string interning system
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <string.h>
#include "pl.h"
#include "pl_string_intern_ext.h"

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

#define PL_STRING_INTERN_BLOCK_SIZE 8096

typedef struct _plStringInternBlock plStringInternBlock;
typedef struct _plStringInternEntry plStringInternEntry;

typedef struct _plStringInternBlock
{
    char                 acBuffer[PL_STRING_INTERN_BLOCK_SIZE];
    plStringInternBlock* ptNextBlock;
    plStringInternEntry* sbtHoles;
} plStringInternBlock;

typedef struct _plStringInternEntry
{
    plStringInternBlock* ptBlock;
    uint16_t             uOffset;
    uint16_t             uSize;
    uint32_t             uRefCount;
} plStringInternEntry;

typedef struct _plStringRepository
{
    plStringInternBlock* ptHeadBlock;
    plHashMap64          tEntryLookup;
    plStringInternEntry* sbtEntries;
} plStringRepository;

typedef struct _plStringInternContext
{
    plStringRepository tDefaultRepo;
} plStringInternContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plStringInternContext* gptStringInternCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

plStringRepository*
pl_string_intern_create_repository(void)
{
    plStringRepository* ptRepo = PL_ALLOC(sizeof(plStringRepository));
    memset(ptRepo, 0, sizeof(plStringRepository));
    return ptRepo;
}

void
pl_string_intern_destroy_repository(plStringRepository* ptRepo)
{

    plStringInternBlock* ptCurrentBlock = ptRepo->ptHeadBlock;
    while(ptCurrentBlock)
    {
        pl_sb_free(ptCurrentBlock->sbtHoles);
        plStringInternBlock* ptNextBlock = ptCurrentBlock->ptNextBlock;
        PL_FREE(ptCurrentBlock);
        ptCurrentBlock = ptNextBlock;
    }

    pl_sb_free(ptRepo->sbtEntries);
    pl_hm_free(&ptRepo->tEntryLookup);
    PL_FREE(ptRepo);
}

const char*
pl_string_intern_intern(plStringRepository* ptRepo, const char* pcString)
{

    if(pcString == NULL)
        return NULL;

    if(ptRepo == NULL)
        ptRepo = &gptStringInternCtx->tDefaultRepo;

    // do hash once
    uint64_t uHash = pl_hm_hash_str(pcString, 0);
    uint64_t uKey = pl_hm_lookup(&ptRepo->tEntryLookup, uHash);

    // check if key exists already
    if(uKey == PL_DS_HASH_INVALID) // doesn't exist
    {
        uKey = pl_hm_get_free_index(&ptRepo->tEntryLookup);
        
        if(uKey == PL_DS_HASH_INVALID) // no free index
        {
            uKey = pl_sb_size(ptRepo->sbtEntries);
            pl_sb_add(ptRepo->sbtEntries);
            ptRepo->sbtEntries[uKey].ptBlock = NULL;
            ptRepo->sbtEntries[uKey].uOffset = 0;
            ptRepo->sbtEntries[uKey].uRefCount = 0;
            ptRepo->sbtEntries[uKey].uSize = 0;
        }
        pl_hm_insert(&ptRepo->tEntryLookup, uHash, uKey);

        const uint16_t uStringLength = (uint16_t)(strlen(pcString) + 1);

        PL_ASSERT(uStringLength < PL_STRING_INTERN_BLOCK_SIZE && "string too long for internment");

        // check if any holes are available
        bool bFoundHole = false;

        plStringInternBlock* ptCurrentBlock = ptRepo->ptHeadBlock;
        while(ptCurrentBlock && !bFoundHole)
        {
            const uint32_t uHoleCount = pl_sb_size(ptCurrentBlock->sbtHoles);

            for(uint32_t i = 0; i < uHoleCount; i++)
            {
                plStringInternEntry* ptHole = &ptCurrentBlock->sbtHoles[i];

                if(ptHole->uSize >= uStringLength)
                {
                    plStringInternEntry* ptEntry = &ptRepo->sbtEntries[uKey];

                    ptEntry->ptBlock   = ptCurrentBlock;
                    ptEntry->uOffset   = ptHole->uOffset;
                    ptEntry->uSize     = uStringLength;
                    ptEntry->uRefCount = 0;

                    if(ptHole->uSize == uStringLength)
                    {
                        pl_sb_del(ptCurrentBlock->sbtHoles, i);
                    }
                    else
                    {
                        ptHole->uOffset += uStringLength;
                        ptHole->uSize   -= uStringLength;
                    }

                    bFoundHole = true;
                    break;
                }
            }

            ptCurrentBlock = ptCurrentBlock->ptNextBlock;
        }

        // add new block
        if(!bFoundHole)
        {
            plStringInternBlock* ptNewBlock = PL_ALLOC(sizeof(plStringInternBlock));
            memset(ptNewBlock, 0, sizeof(plStringInternBlock));

            ptNewBlock->ptNextBlock = ptRepo->ptHeadBlock;
            ptRepo->ptHeadBlock = ptNewBlock;

            // add hole
            plStringInternEntry tHole = {
                .ptBlock = ptNewBlock,
                .uSize   = PL_STRING_INTERN_BLOCK_SIZE - uStringLength,
                .uOffset = uStringLength
            };
            pl_sb_push(ptNewBlock->sbtHoles, tHole);

            ptRepo->sbtEntries[uKey].ptBlock = ptNewBlock;
            ptRepo->sbtEntries[uKey].uOffset = 0;
            ptRepo->sbtEntries[uKey].uRefCount = 0;
            ptRepo->sbtEntries[uKey].uSize = uStringLength;
        }

        if(&ptRepo->sbtEntries[uKey].ptBlock->acBuffer[ptRepo->sbtEntries[uKey].uOffset] != pcString)
        {
            strncpy(&ptRepo->sbtEntries[uKey].ptBlock->acBuffer[ptRepo->sbtEntries[uKey].uOffset], pcString, uStringLength);
        }
        else
        {
            PL_ASSERT(false);
        }
    }

    ptRepo->sbtEntries[uKey].uRefCount++;
    return &ptRepo->sbtEntries[uKey].ptBlock->acBuffer[ptRepo->sbtEntries[uKey].uOffset];
}

void
pl_string_intern_remove(plStringRepository* ptRepo, const char* pcString)
{

    if(pcString == NULL)
        return;

    if(ptRepo == NULL)
        ptRepo = &gptStringInternCtx->tDefaultRepo;

    // do hash once
    uint64_t uHash = pl_hm_hash_str(pcString, 0);
    uint64_t uKey = pl_hm_lookup(&ptRepo->tEntryLookup, uHash);

    // check if key exists already
    if(uKey == PL_DS_HASH_INVALID) // doesn't exist
    {
        PL_ASSERT(false && "string does not exist in this repository");
    }
    else
    {
        ptRepo->sbtEntries[uKey].uRefCount--;

        if(ptRepo->sbtEntries[uKey].uRefCount == 0)
        {
            pl_hm_remove(&ptRepo->tEntryLookup, uHash);

            // add hole
            plStringInternEntry tHole = {
                .ptBlock = ptRepo->sbtEntries[uKey].ptBlock,
                .uSize   = ptRepo->sbtEntries[uKey].uSize,
                .uOffset = ptRepo->sbtEntries[uKey].uOffset
            };
            // pl_sb_push(ptRepo->sbtHoles, tHole);

            plStringInternBlock* ptCurrentBlock = ptRepo->sbtEntries[uKey].ptBlock;

            uint32_t uHoleCount = pl_sb_size(ptCurrentBlock->sbtHoles);
            
            bool bFoundGreaterHoleOffset = false;

            for(uint32_t i = 0; i < uHoleCount; i++)
            {
                plStringInternEntry* ptPrevHole = NULL;
                if(i > 0)
                    ptPrevHole = &ptCurrentBlock->sbtHoles[i - 1];

                // check if hole can be placed
                if(ptCurrentBlock->sbtHoles[i].uOffset > tHole.uOffset)
                {
                    bool bMergeForward = false;

                    // see if we can merge with next block
                    if(ptCurrentBlock->sbtHoles[i].uOffset == tHole.uOffset + tHole.uSize)
                    {
                        ptCurrentBlock->sbtHoles[i].uOffset = tHole.uOffset;
                        ptCurrentBlock->sbtHoles[i].uSize += tHole.uSize;
                        tHole = ptCurrentBlock->sbtHoles[i];
                        bMergeForward = true;
                    }

                    // see if we can merge with previous block
                    if(ptPrevHole && ptPrevHole->uOffset + ptPrevHole->uSize == tHole.uOffset)
                    {
                        ptPrevHole->uSize += tHole.uSize;

                        if(bMergeForward)
                        {
                            pl_sb_del(ptCurrentBlock->sbtHoles, i);
                        }
                    }
                    else if(!bMergeForward)
                    {
                        pl_sb_insert(ptCurrentBlock->sbtHoles, i, tHole);
                    }
                    bFoundGreaterHoleOffset = true;
                    break;
                }
            }

            if(!bFoundGreaterHoleOffset)
            {
                uHoleCount = pl_sb_size(ptCurrentBlock->sbtHoles);

                if(uHoleCount > 0)
                {
                    plStringInternEntry* ptLast = &ptCurrentBlock->sbtHoles[uHoleCount - 1];

                    if(ptLast->uOffset + ptLast->uSize == tHole.uOffset)
                    {
                        ptLast->uSize += tHole.uSize;
                        return;
                    }
                }

                pl_sb_push(ptCurrentBlock->sbtHoles, tHole);
            }
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_string_intern_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plStringInternI tApi = {
        .create_repository  = pl_string_intern_create_repository,
        .destroy_repository = pl_string_intern_destroy_repository,
        .intern             = pl_string_intern_intern,
        .remove             = pl_string_intern_remove
    };
    pl_set_api(ptApiRegistry, plStringInternI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptStringInternCtx = ptDataRegistry->get_data("plStringInternContext");
    }
    else // first load
    {
        static plStringInternContext tCtx = {0};
        gptStringInternCtx = &tCtx;
        ptDataRegistry->set_data("plStringInternContext", gptStringInternCtx);
    }
}

void
pl_unload_string_intern_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    plStringInternBlock* ptCurrentBlock = gptStringInternCtx->tDefaultRepo.ptHeadBlock;
    while(ptCurrentBlock)
    {
        pl_sb_free(ptCurrentBlock->sbtHoles);
        plStringInternBlock* ptNextBlock = ptCurrentBlock->ptNextBlock;
        PL_FREE(ptCurrentBlock);
        ptCurrentBlock = ptNextBlock;
    }

    pl_sb_free(gptStringInternCtx->tDefaultRepo.sbtEntries);
    pl_hm_free(&gptStringInternCtx->tDefaultRepo.tEntryLookup);

    const plStringInternI* ptApi = pl_get_api_latest(ptApiRegistry, plStringInternI);
    ptApiRegistry->remove_api(ptApi);
}