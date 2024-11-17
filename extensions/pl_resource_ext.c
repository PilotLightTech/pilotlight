//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "pl.h"
#include "pl_resource_ext.h"
#include "pl_ds.h"
#include "pl_log.h"

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

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plResource
{
    char                acName[PL_MAX_NAME_LENGTH];
    plResourceLoadFlags tFlags;
    uint8_t*            puFileData;
    size_t              szFileDataSize;
    void*               pBufferData;
    size_t              szBufferDataSize;
} plResource;

typedef struct _plResourceManager
{
    plResource* sbtResources;
    uint32_t*   sbtResourceGenerations;
    plHashMap*  ptNameHashmap;
} plResourceManager;

// resource retrieval
static const void* pl_resource_get_file_data(plResourceHandle tResourceHandle, size_t* pszDataSize);
static const void* pl_resource_get_buffer_data(plResourceHandle tResourceHandle, size_t* pszDataSize);

static void pl_set_buffer_data(plResourceHandle tResourceHandle, size_t szDataSize, void* pData);

//  resources
static plResourceHandle pl_load_resource           (const char* pcName, plResourceLoadFlags tFlags, uint8_t* puData, size_t szDataSize);
static bool             pl_is_resource_loaded      (const char* pcName);
static bool             pl_is_resource_valid       (plResourceHandle tResourceHandle);
static void             pl_unload_resource         (plResourceHandle tResourceHandle);

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plResourceManager* gptResourceManager = NULL;

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static const void*
pl_resource_get_file_data(plResourceHandle tResourceHandle, size_t* pszDataSize)
{
    if(pszDataSize)
        *pszDataSize = 0;
    
    if(tResourceHandle.uGeneration != gptResourceManager->sbtResourceGenerations[tResourceHandle.uGeneration])
        return NULL;

    const plResource* ptResource = &gptResourceManager->sbtResources[tResourceHandle.uIndex];

    if(pszDataSize)
        *pszDataSize = ptResource->szFileDataSize;

    return ptResource->puFileData;
}

static const void*
pl_resource_get_buffer_data(plResourceHandle tResourceHandle, size_t* pszDataSize)
{
    if(pszDataSize)
        *pszDataSize = 0;
    
    if(tResourceHandle.uGeneration != gptResourceManager->sbtResourceGenerations[tResourceHandle.uGeneration])
        return NULL;

    const plResource* ptResource = &gptResourceManager->sbtResources[tResourceHandle.uIndex];

    if(pszDataSize)
        *pszDataSize = ptResource->szBufferDataSize;

    return ptResource->pBufferData;
}

static void
pl_set_buffer_data(plResourceHandle tResourceHandle, size_t szDataSize, void* pData)
{
    plResource* ptResource = &gptResourceManager->sbtResources[tResourceHandle.uIndex];
    if(ptResource->pBufferData)
        PL_FREE(ptResource->pBufferData);
    ptResource->pBufferData = pData;
    ptResource->szBufferDataSize = szDataSize;
}

static plResourceHandle
pl_load_resource(const char* pcName, plResourceLoadFlags tFlags, uint8_t* puData, size_t szDataSize)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName);
    if(pl_hm_has_key(gptResourceManager->ptNameHashmap, ulHash))
    {
        uint64_t ulExistingSlot = pl_hm_lookup(gptResourceManager->ptNameHashmap, ulHash);

        plResourceHandle tResource = {
            .uIndex      = (uint32_t)ulExistingSlot,
            .uGeneration = gptResourceManager->sbtResourceGenerations[ulExistingSlot]
        };
        return tResource;
    }

    plResource tResource = {
        .tFlags         = tFlags,
        .szFileDataSize = szDataSize,
        .puFileData     = puData
    };

    strncpy(tResource.acName, pcName, PL_MAX_NAME_LENGTH);

    uint64_t uIndex = pl_hm_get_free_index(gptResourceManager->ptNameHashmap);
    if(uIndex == UINT64_MAX)
    {
        uIndex = pl_sb_size(gptResourceManager->sbtResourceGenerations);
        pl_sb_push(gptResourceManager->sbtResourceGenerations, 0);
        pl_sb_add(gptResourceManager->sbtResources);

    }

    if(tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_DATA)
    {
        tResource.puFileData = PL_ALLOC(szDataSize);
        memcpy(tResource.puFileData, puData, szDataSize);
    }  
    pl_hm_insert_str(gptResourceManager->ptNameHashmap, pcName, uIndex);

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
            PL_FREE(ptResource->puFileData);
        }
        memset(ptResource, 0, sizeof(plResource));

        gptResourceManager->sbtResourceGenerations[tResourceHandle.uIndex]++;
        pl_hm_remove_str(gptResourceManager->ptNameHashmap, ptResource->acName);
    }
}

static bool
pl_is_resource_loaded(const char* pcName)
{
    return pl_hm_has_key_str(gptResourceManager->ptNameHashmap, pcName);    
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

static void
pl_load_resource_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plResourceI tApi = {
        .get_file_data      = pl_resource_get_file_data,
        .get_buffer_data    = pl_resource_get_buffer_data,
        .set_buffer_data    = pl_set_buffer_data,
        .load_resource      = pl_load_resource,
        .is_resource_loaded = pl_is_resource_loaded,
        .is_resource_valid  = pl_is_resource_valid,
        .unload_resource    = pl_unload_resource
    };
    pl_set_api(ptApiRegistry, plResourceI, &tApi);
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptResourceManager = ptDataRegistry->get_data("plResourceManager");
    }
    else
    {
        static plResourceManager gtResourceManager = {0};
        gptResourceManager = &gtResourceManager;
        ptDataRegistry->set_data("plResourceManager", gptResourceManager);
    }
}

static void
pl_unload_resource_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plResourceI* ptApi = pl_get_api_latest(ptApiRegistry, plResourceI);
    ptApiRegistry->remove_api(ptApi);

    for(uint32_t i = 0; i < pl_sb_size(gptResourceManager->sbtResources); i++)
    {
        if(gptResourceManager->sbtResources[i].tFlags & PL_RESOURCE_LOAD_FLAG_RETAIN_DATA)
        {
            PL_FREE(gptResourceManager->sbtResources[i].puFileData);
        }
        if(gptResourceManager->sbtResources[i].pBufferData)
        {
            PL_FREE(gptResourceManager->sbtResources[i].pBufferData);
        }
    }

    pl_sb_free(gptResourceManager->sbtResourceGenerations);
    pl_sb_free(gptResourceManager->sbtResources);
    pl_hm_free(gptResourceManager->ptNameHashmap);
}