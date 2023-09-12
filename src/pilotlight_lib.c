#include "pilotlight.h"

#include <stdlib.h>
#undef PL_DS_ALLOC
#undef PL_DS_ALLOC_INDIRECT
#undef PL_DS_FREE
#undef PL_REALLOC
#define PL_DS_ALLOC(x) malloc((x))
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) malloc((x))
#define PL_DS_FREE(x)  free((x))
#include "pl_ds.h"

#define PL_LOG_IMPLEMENTATION
#include "pl_log.h"
#undef PL_LOG_IMPLEMENTATION

#define PL_PROFILE_IMPLEMENTATION
#include "pl_profile.h"
#undef PL_PROFILE_IMPLEMENTATION

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
#undef PL_STRING_IMPLEMENTATION

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define PL_MEMORY_IMPLEMENTATION
#include "pl_memory.h"
#undef PL_MEMORY_IMPLEMENTATION

static plMemoryContext* gptMemoryContext = NULL;

void
pl_set_memory_context(plMemoryContext* ptMemoryContext)
{
    gptMemoryContext = ptMemoryContext;
}

plMemoryContext*
pl_get_memory_context(void)
{
    return gptMemoryContext;
}

void*
pl_realloc(void* pBuffer, size_t szSize, const char* pcFile, int iLine)
{
    void* pNewBuffer = NULL;

    if(szSize > 0)
    {
        gptMemoryContext->szActiveAllocations++;
        pNewBuffer = malloc(szSize);
        memset(pNewBuffer, 0, szSize);

        const uint64_t ulHash = pl_hm_hash(&pNewBuffer, sizeof(void*), 1);

        uint64_t ulFreeIndex = pl_hm_get_free_index(gptMemoryContext->ptHashMap);
        if(ulFreeIndex == UINT64_MAX)
        {
            pl_sb_push(gptMemoryContext->sbtAllocations, (plAllocationEntry){0});
            ulFreeIndex = pl_sb_size(gptMemoryContext->sbtAllocations) - 1;
        }
        pl_hm_insert(gptMemoryContext->ptHashMap, ulHash, ulFreeIndex);
        gptMemoryContext->sbtAllocations[ulFreeIndex].iLine = iLine;
        gptMemoryContext->sbtAllocations[ulFreeIndex].pcFile = pcFile;
        gptMemoryContext->sbtAllocations[ulFreeIndex].pAddress = pNewBuffer;
        gptMemoryContext->sbtAllocations[ulFreeIndex].szSize = szSize;
        gptMemoryContext->szAllocationCount++;
    }


    if(pBuffer) // free
    {
        const uint64_t ulHash = pl_hm_hash(&pBuffer, sizeof(void*), 1);
        const bool bDataExists = pl_hm_has_key(gptMemoryContext->ptHashMap, ulHash);

        if(bDataExists)
        {
            const uint64_t ulIndex = pl_hm_lookup(gptMemoryContext->ptHashMap, ulHash);

            if(pNewBuffer)
            {
                memcpy(pNewBuffer, pBuffer, gptMemoryContext->sbtAllocations[ulIndex].szSize);
            }
            gptMemoryContext->sbtAllocations[ulIndex].pAddress = NULL;
            gptMemoryContext->sbtAllocations[ulIndex].szSize = 0;
            pl_hm_remove(gptMemoryContext->ptHashMap, ulHash);
            gptMemoryContext->szAllocationFrees++;
            gptMemoryContext->szActiveAllocations--;
        }
        else
        {
            PL_ASSERT(false);
        }
        free(pBuffer);
    }
    return pNewBuffer;
}

// ui
#include "pl_ui.c"
#include "pl_ui_widgets.c"
#include "pl_ui_demo.c"
#include "pl_ui_draw.c"