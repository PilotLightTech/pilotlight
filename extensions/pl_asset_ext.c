/*
   pl_asset_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_asset_ext.h"

// extensions
#include "pl_log_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_platform_ext.h"

// libraries
#include "pl_memory.h"
#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plLogI*           gptLog           = NULL;
    static const plMemoryI*        gptMemory        = NULL;
    static const plStringInternI*  gptString        = NULL;
    static const plVirtualMemoryI* gptVirtualMemory = NULL;

    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

// libs
#include "pl_ds.h"

#define PL_ASSET_VIRTUAL_RESERVE_SIZE (64ull * 1024ull * 1024ull)
#define PL_ASSET_COMMIT_GRANULARITY (64 * 1024)

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAsset
{
    plAssetTypeKey tType;
    plAssetFlags   eFlags;
    const char*    pcName;
    const char*    pcSource;
    uint32_t       uDataIndex;
} plAsset;

typedef struct _plAssetRegisteredType
{
    const char*     pcFileExtension; // string intern
    plAssetTypeDesc tDesc;
    uint32_t*       sbtFreeSlots;
    void*           pAssets;
    size_t          szReserved;
    size_t          szCommitted;
    uint32_t        uCount;
} plAssetRegisteredType;

typedef struct _plAssetContext
{
    bool                   bFinalized;
    plTempAllocator        tTempAllocator;
    plStringRepository*    ptStringRepo;
    plAsset*               sbtAssets;
    plHashMap              tAssetLookup;
    uint32_t*              sbtAssetGenerations;
    plAssetRegisteredType* sbtTypeDescriptions;
    plAssetTypeDesc*       sbtTypeUserDescriptions;
} plAssetContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plAssetContext* gptAssetCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

static void
pl__ensure_asset_capacity(plAssetRegisteredType* ptType, uint32_t uIndex)
{
    const size_t szPageSize = gptVirtualMemory->get_page_size();

    if(ptType->pAssets == NULL)
    {
        ptType->szReserved = PL_ASSET_VIRTUAL_RESERVE_SIZE;
        ptType->pAssets = gptVirtualMemory->reserve(ptType->szReserved);
        PL_ASSERT(ptType->pAssets);
    }

    const size_t szRequired = (size_t)(uIndex + 1) * ptType->tDesc.szSize;

    if(szRequired > ptType->szCommitted)
    {
        // Could also grow by larger chunks here.
        size_t szNewCommitted = (szRequired + PL_ASSET_COMMIT_GRANULARITY - 1) & ~(PL_ASSET_COMMIT_GRANULARITY - 1);
        PL_ASSERT(szNewCommitted <= ptType->szReserved);
        void* pCommitAddress = (char*)ptType->pAssets + ptType->szCommitted;
        size_t szCommit = szNewCommitted - ptType->szCommitted;
        void* pResult = gptVirtualMemory->commit(pCommitAddress, szCommit);
        PL_ASSERT(pResult);
        ptType->szCommitted = szNewCommitted;
    }
}

plAssetTypeKey
pl_asset_register_type(plAssetTypeDesc tDesc)
{
    PL_ASSERT(!gptAssetCtx->bFinalized && "asset setup already finalized!");

    if(gptAssetCtx->bFinalized)
        return UINT32_MAX;

    plAssetRegisteredType tRegisteredType = {
        .tDesc = tDesc,
        .sbtFreeSlots = NULL,
        .pcFileExtension = gptString->intern_ex(tDesc.pcFileExtension, gptAssetCtx->ptStringRepo)
    };
    pl_sb_push(gptAssetCtx->sbtTypeDescriptions, tRegisteredType);
    pl_sb_push(gptAssetCtx->sbtTypeUserDescriptions, tDesc);
    return pl_sb_size(gptAssetCtx->sbtTypeDescriptions) - 1;
}

const plAssetTypeDesc*
pl_asset_get_type_description(plAssetTypeKey tKey)
{
    return &gptAssetCtx->sbtTypeDescriptions[tKey].tDesc;
}

uint32_t
pl_asset_get_type_descriptions(const plAssetTypeDesc** pptAssetDescOut)
{
    *pptAssetDescOut = gptAssetCtx->sbtTypeUserDescriptions;
    return pl_sb_size(gptAssetCtx->sbtTypeUserDescriptions);
}

void
pl_asset_initialize(plAssetInit tInit)
{
    gptAssetCtx->ptStringRepo = gptString->create_repository();
    pl_sb_push(gptAssetCtx->sbtAssetGenerations, 4194303);
    pl_sb_add(gptAssetCtx->sbtAssets);
}

void
pl_asset_finalize(void)
{
    gptAssetCtx->bFinalized = true;
}

void
pl_asset_cleanup(void)
{
    gptString->destroy_repository(gptAssetCtx->ptStringRepo);
    gptAssetCtx->ptStringRepo = NULL;

    const uint32_t uAssetTypeCount = pl_sb_size(gptAssetCtx->sbtTypeDescriptions);
    for(uint32_t i = 0; i < uAssetTypeCount; i++)
    {
        plAssetRegisteredType* ptType = &gptAssetCtx->sbtTypeDescriptions[i];
        if(ptType->tDesc.cleanup)
        {
            for(uint32_t j = 0; j < ptType->uCount; j++)
            {
                ptType->tDesc.cleanup(&((char*)ptType->pAssets)[j * ptType->tDesc.szSize]);
            }
        }
        pl_sb_free(ptType->sbtFreeSlots);
        if(ptType->pAssets)
        {
            gptVirtualMemory->free(ptType->pAssets, ptType->szReserved);
        }
    }

    pl_hm_free(&gptAssetCtx->tAssetLookup);
    pl_sb_free(gptAssetCtx->sbtAssetGenerations);
    pl_sb_free(gptAssetCtx->sbtTypeDescriptions);
    pl_sb_free(gptAssetCtx->sbtTypeUserDescriptions);
    pl_sb_free(gptAssetCtx->sbtAssets);
    pl_temp_allocator_free(&gptAssetCtx->tTempAllocator);
}

plAssetHandle
pl_asset_create(const plAssetDesc* ptDesc, const void* pData)
{
    const uint64_t ulHash = pl_hm_hash_str(ptDesc->pcPath, 0);
    uint64_t ulExistingSlot = 0;
    if(pl_hm_has_key_ex(&gptAssetCtx->tAssetLookup, ulHash, &ulExistingSlot))
    {
        plAssetHandle tAsset = {0};
        tAsset.uIndex      = (uint32_t)ulExistingSlot;
        tAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[ulExistingSlot];
        return tAsset;
    }

    uint64_t uIndex = pl_hm_get_free_index(&gptAssetCtx->tAssetLookup);
    if(uIndex == PL_DS_HASH_INVALID)
    {
        uIndex = pl_sb_size(gptAssetCtx->sbtAssetGenerations);
        pl_sb_push(gptAssetCtx->sbtAssetGenerations, 0);
        pl_sb_add(gptAssetCtx->sbtAssets);
    }
    pl_hm_insert_str(&gptAssetCtx->tAssetLookup, ptDesc->pcPath, uIndex);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[uIndex];
    ptAsset->tType = ptDesc->tType;
    ptAsset->eFlags = ptDesc->eFlags;
    ptAsset->pcName = gptString->intern_ex(ptDesc->pcPath, gptAssetCtx->ptStringRepo);
    if(ptDesc->pcSourcePath)
        ptAsset->pcSource = gptString->intern_ex(ptDesc->pcSourcePath, gptAssetCtx->ptStringRepo);
    ptAsset->uDataIndex = UINT32_MAX;

    plAssetHandle tNewAsset = {0};
    tNewAsset.uIndex      = (uint32_t)uIndex;
    tNewAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[uIndex];

    if(pData)
    {
        plAssetRegisteredType* ptType = &gptAssetCtx->sbtTypeDescriptions[ptDesc->tType];

        if(ptAsset->uDataIndex == UINT32_MAX)
        {
            if(pl_sb_size(ptType->sbtFreeSlots) > 0)
            {
                ptAsset->uDataIndex = pl_sb_pop(ptType->sbtFreeSlots);
            }
            else
            {
                ptAsset->uDataIndex = ptType->uCount++;
                pl__ensure_asset_capacity(ptType, ptAsset->uDataIndex);
            }
            memcpy(&((char*)ptType->pAssets)[ptAsset->uDataIndex * ptType->tDesc.szSize], pData, ptType->tDesc.szSize);
        }
    }
    return tNewAsset;
}

void
pl_asset_destroy(plAssetHandle tHandle)
{
    if(!pl_asset_is_valid(tHandle))
        return;

    plAssetTypeKey tType = pl_asset_get_type_key(tHandle);

    uint32_t uDataIndex = gptAssetCtx->sbtAssets[tHandle.uIndex].uDataIndex;

    plAssetRegisteredType* ptType = &gptAssetCtx->sbtTypeDescriptions[tType];
    pl_hm_remove_str(&gptAssetCtx->tAssetLookup, gptAssetCtx->sbtAssets[tHandle.uIndex].pcName);

    if(ptType->tDesc.cleanup)
    {
        ptType->tDesc.cleanup(&((char*)ptType->pAssets)[uDataIndex * ptType->tDesc.szSize]);
    }
    pl_sb_push(ptType->sbtFreeSlots, uDataIndex);

    gptAssetCtx->sbtAssetGenerations[tHandle.uIndex]++;
    gptAssetCtx->sbtAssets[tHandle.uIndex].uDataIndex = UINT32_MAX;
    gptString->remove_ex(gptAssetCtx->sbtAssets[tHandle.uIndex].pcName, gptAssetCtx->ptStringRepo);
    gptAssetCtx->sbtAssets[tHandle.uIndex].pcName = NULL;
    if(gptAssetCtx->sbtAssets[tHandle.uIndex].pcSource)
        gptString->remove_ex(gptAssetCtx->sbtAssets[tHandle.uIndex].pcSource, gptAssetCtx->ptStringRepo);
    gptAssetCtx->sbtAssets[tHandle.uIndex].pcSource = NULL;
}

plAssetHandle
pl_asset_load(const char* pcFile)
{
    const uint64_t ulHash = pl_hm_hash_str(pcFile, 0);
    uint64_t ulExistingSlot = 0;
    if(pl_hm_has_key_ex(&gptAssetCtx->tAssetLookup, ulHash, &ulExistingSlot))
    {
        plAssetHandle tAsset = {0};
        tAsset.uIndex      = (uint32_t)ulExistingSlot;
        tAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[ulExistingSlot];
        return tAsset;
    }

    // find new asset home slot
    uint64_t uIndex = pl_hm_get_free_index(&gptAssetCtx->tAssetLookup);
    if(uIndex == PL_DS_HASH_INVALID)
    {
        uIndex = pl_sb_size(gptAssetCtx->sbtAssetGenerations);
        pl_sb_push(gptAssetCtx->sbtAssetGenerations, 0);
        pl_sb_add(gptAssetCtx->sbtAssets);
    }
    pl_hm_insert_str(&gptAssetCtx->tAssetLookup, pcFile, uIndex);

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[uIndex];
    ptAsset->tType = UINT32_MAX;
    ptAsset->eFlags = PL_ASSET_FLAG_NONE;
    ptAsset->pcName = gptString->intern_ex(pcFile, gptAssetCtx->ptStringRepo);

    char acFileExtension[32] = {0};
    pl_str_get_file_extension(pcFile, acFileExtension, 32);

    const char* pcFileExtension = gptString->intern_ex(acFileExtension, gptAssetCtx->ptStringRepo);

    const uint32_t uAssetTypeCount = pl_sb_size(gptAssetCtx->sbtTypeDescriptions);
    plAssetTypeKey tType = UINT32_MAX;
    for(uint32_t i = 0; i < uAssetTypeCount; i++)
    {
        if(pcFileExtension == gptAssetCtx->sbtTypeDescriptions[i].pcFileExtension)
        {
            tType = i;
            break;
        }
    }

    plAssetRegisteredType* ptType = &gptAssetCtx->sbtTypeDescriptions[tType];

    if(pl_sb_size(ptType->sbtFreeSlots) > 0)
    {
        ptAsset->uDataIndex = pl_sb_pop(ptType->sbtFreeSlots);
    }
    else
    {
        ptAsset->uDataIndex = ptType->uCount++;
        pl__ensure_asset_capacity(ptType, ptAsset->uDataIndex);
    }
    ptAsset->tType = tType;
    bool bResult = ptType->tDesc.deserialize(ptAsset->pcName, &((char*)ptType->pAssets)[ptAsset->uDataIndex * ptType->tDesc.szSize]);
    PL_ASSERT(bResult);
    
    gptString->remove_ex(pcFileExtension, gptAssetCtx->ptStringRepo);

    plAssetHandle tNewAsset = {0};
    tNewAsset.uIndex      = (uint32_t)uIndex;
    tNewAsset.uGeneration = gptAssetCtx->sbtAssetGenerations[uIndex];
    return tNewAsset;
}

bool
pl_asset_save(plAssetHandle tHandle, plAssetEncoding tEncoding)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return false;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    plAssetRegisteredType* ptType = &gptAssetCtx->sbtTypeDescriptions[ptAsset->tType];
    if(tEncoding == PL_ASSET_ENCODING_AUTO)
    {
        tEncoding = ptType->tDesc.eDefaultEncoding;
    }
    return ptType->tDesc.serialize(ptAsset->pcName, &((char*)ptType->pAssets)[ptAsset->uDataIndex * ptType->tDesc.szSize], tEncoding);
}

plAssetHandle
pl_asset_find(const char* pcName)
{
    if(pcName)
    {
        uint64_t uIndex = 0;
        if(pl_hm_has_key_str_ex(&gptAssetCtx->tAssetLookup, pcName, &uIndex))
        {
            return (plAssetHandle){
                .uIndex = (uint32_t)uIndex,
                .uGeneration = gptAssetCtx->sbtAssetGenerations[uIndex]
            };
        }
    }
    return (plAssetHandle){0};
}

bool
pl_asset_is_valid(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return false;
    return true;
}

plAssetTypeKey
pl_asset_get_type_key(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return UINT32_MAX;
    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return ptAsset->tType;
}

const char*
pl_asset_get_path(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return ptAsset->pcName;
}

const char*
pl_asset_get_source_path(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    return ptAsset->pcSource;
}

void*
pl_asset_get_data(plAssetHandle tHandle)
{
    if(tHandle.uGeneration != gptAssetCtx->sbtAssetGenerations[tHandle.uIndex])
        return NULL;

    plAsset* ptAsset = &gptAssetCtx->sbtAssets[tHandle.uIndex];
    plAssetRegisteredType* ptType = &gptAssetCtx->sbtTypeDescriptions[ptAsset->tType];
    return &((char*)ptType->pAssets)[ptAsset->uDataIndex * ptType->tDesc.szSize];
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_asset_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plAssetI tApi = {
        .initialize            = pl_asset_initialize,
        .finalize              = pl_asset_finalize,
        .cleanup               = pl_asset_cleanup,
        .load                  = pl_asset_load,
        .find                  = pl_asset_find,
        .create                = pl_asset_create,
        .destroy               = pl_asset_destroy,
        .get_data              = pl_asset_get_data,
        .save                  = pl_asset_save,
        .get_path              = pl_asset_get_path,
        .get_source_path       = pl_asset_get_source_path,
        .is_valid              = pl_asset_is_valid,
        .register_type         = pl_asset_register_type,
        .get_type_key          = pl_asset_get_type_key,
        .get_type_description  = pl_asset_get_type_description,
        .get_type_descriptions = pl_asset_get_type_descriptions,
    };
    pl_set_api(ptApiRegistry, plAssetI, &tApi);

    gptLog           = pl_get_api_latest(ptApiRegistry, plLogI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptVirtualMemory = pl_get_api_latest(ptApiRegistry, plVirtualMemoryI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptAssetCtx = ptDataRegistry->get_data("plAssetContext");
    }
    else // first load
    {
        static plAssetContext tCtx = {0};
        gptAssetCtx = &tCtx;
        ptDataRegistry->set_data("plAssetContext", gptAssetCtx);
    }
}

void
pl_unload_asset_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plAssetI* ptApi = pl_get_api_latest(ptApiRegistry, plAssetI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif
