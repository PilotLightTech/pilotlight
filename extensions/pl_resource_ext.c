//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_resource_ext.h"
#include "pl_ds.h"
#include "pl_log.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plResource
{
    char                acName[PL_MAX_NAME_LENGTH];
    plResourceLoadFlags tFlags;
    char*               pcFileData;
    size_t              szFileDataSize;
} plResource;

typedef struct _plResourceManager
{
    plResource* sbtResources;
    uint32_t*   sbtResourceGenerations;
    plHashMap   tNameHashMap;
} plResourceManager;

// resource retrieval
static const char* pl_resource_get_file_data(plResourceHandle tResourceHandle, size_t* pszDataSize);

//  resources
static plResourceHandle pl_load_resource           (const char* pcName, plResourceLoadFlags tFlags, char* pcData, size_t szDataSize);
static bool             pl_is_resource_loaded      (const char* pcName);
static bool             pl_is_resource_valid       (plResourceHandle tResourceHandle);
static void             pl_unload_resource         (plResourceHandle tResourceHandle);

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// apis
static const plApiRegistryI*   gptApiRegistry  = NULL;
static const plDataRegistryI*  ptDataRegistry  = NULL;

static plResourceManager* gptResourceManager = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plResourceI*
pl_load_resource_api(void)
{
    static const plResourceI tApi = {
        .get_file_data      = pl_resource_get_file_data,
        .load_resource      = pl_load_resource,
        .is_resource_loaded = pl_is_resource_loaded,
        .is_resource_valid  = pl_is_resource_valid,
        .unload_resource    = pl_unload_resource
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static const char*
pl_resource_get_file_data(plResourceHandle tResourceHandle, size_t* pszDataSize)
{
    if(pszDataSize)
        *pszDataSize = 0;
    
    if(tResourceHandle.uGeneration != gptResourceManager->sbtResourceGenerations[tResourceHandle.uGeneration])
        return NULL;

    const plResource* ptResource = &gptResourceManager->sbtResources[tResourceHandle.uIndex];

    if(pszDataSize)
        *pszDataSize = ptResource->szFileDataSize;

    return ptResource->pcFileData;
}

static plResourceHandle
pl_load_resource(const char* pcName, plResourceLoadFlags tFlags, char* pcData, size_t szDataSize)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName);
    if(pl_hm_has_key(&gptResourceManager->tNameHashMap, ulHash))
    {
        uint64_t ulExistingSlot = pl_hm_lookup(&gptResourceManager->tNameHashMap, ulHash);

        plResourceHandle tResource = {
            .uIndex      = (uint32_t)ulExistingSlot,
            .uGeneration = gptResourceManager->sbtResourceGenerations[ulExistingSlot]
        };
        return tResource;
    }

    plResource tResource = {
        .tFlags         = tFlags,
        .szFileDataSize = szDataSize,
        .pcFileData     = pcData
    };

    strncpy(tResource.acName, pcName, PL_MAX_NAME_LENGTH);

    uint64_t uIndex = pl_hm_get_free_index(&gptResourceManager->tNameHashMap);
    if(uIndex == UINT64_MAX)
    {
        uIndex = pl_sb_size(gptResourceManager->sbtResourceGenerations);
        pl_sb_push(gptResourceManager->sbtResourceGenerations, 0);
        pl_sb_add(gptResourceManager->sbtResources);

    }

    if(tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_DATA)
    {
        tResource.pcFileData = PL_ALLOC(szDataSize);
        memcpy(tResource.pcFileData, pcData, szDataSize);
    }  
    pl_hm_insert_str(&gptResourceManager->tNameHashMap, pcName, uIndex);

    plResourceHandle tNewResource = {
        .uIndex      = (uint32_t)uIndex,
        .uGeneration = gptResourceManager->sbtResourceGenerations[uIndex]
    };

    gptResourceManager->sbtResources[uIndex] = tResource;

    return tNewResource;
};

static void
pl_unload_resource(plResourceHandle tResourceHandle)
{
    PL_ASSERT(tResourceHandle.uGeneration == gptResourceManager->sbtResourceGenerations[tResourceHandle.uGeneration]);
    
    if(tResourceHandle.uGeneration == gptResourceManager->sbtResourceGenerations[tResourceHandle.uGeneration])
    {
        plResource* ptResource = &gptResourceManager->sbtResources[tResourceHandle.uIndex];

        if(ptResource->tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_DATA)
        {
            PL_FREE(ptResource->pcFileData);
        }
        memset(ptResource, 0, sizeof(plResource));

        gptResourceManager->sbtResourceGenerations[tResourceHandle.uIndex]++;
        pl_hm_remove_str(&gptResourceManager->tNameHashMap, ptResource->acName);
    }
}

static bool
pl_is_resource_loaded(const char* pcName)
{
    return pl_hm_has_key_str(&gptResourceManager->tNameHashMap, pcName);    
}

static bool
pl_is_resource_valid(plResourceHandle tResourceHandle)
{
    if(tResourceHandle.uGeneration == UINT32_MAX ||  tResourceHandle.uIndex == UINT32_MAX)
        return false;
    return tResourceHandle.uGeneration == gptResourceManager->sbtResourceGenerations[tResourceHandle.uIndex];
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    gptApiRegistry = ptApiRegistry;
    ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_log_context(ptDataRegistry->get_data("log"));

    if(bReload)
    {
        gptResourceManager = ptDataRegistry->get_data("resource manager");
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_RESOURCE), pl_load_resource_api());
    }
    else
    {
        gptResourceManager = PL_ALLOC(sizeof(plResourceManager));
        memset(gptResourceManager, 0, sizeof(plResourceManager));
        ptDataRegistry->set_data("resource manager", gptResourceManager);
        ptApiRegistry->add(PL_API_RESOURCE, pl_load_resource_api());
    }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
    for(uint32_t i = 0; i < pl_sb_size(gptResourceManager->sbtResources); i++)
    {
        if(gptResourceManager->sbtResources[i].tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_DATA)
        {
            PL_FREE(gptResourceManager->sbtResources[i].pcFileData);
        }
    }

    pl_sb_free(gptResourceManager->sbtResourceGenerations);
    pl_sb_free(gptResourceManager->sbtResources);
    pl_hm_free(&gptResourceManager->tNameHashMap);

    PL_FREE(gptResourceManager);

    ptDataRegistry->set_data("resource manager", NULL);
}