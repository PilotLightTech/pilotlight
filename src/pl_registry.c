/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include <stdbool.h> // bool
#include <string.h>  // strcmp
#include "pl_registry.h"
#include "pl_ds.h"
#include "pl_os.h"

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plDataEntry
{
    const char* pcName;
    void*       pData;
} plDataEntry;

typedef struct _plDataRegistry
{
    plHashMap    tHashMap;
    plDataEntry* sbDataEntries;
} plDataRegistry;

typedef struct _plApiEntry
{
    const char*           pcName;
    void*                 pInterface;
    ptApiUpdateCallback * sbSubscribers;
    void**                sbUserData;
} plApiEntry;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// data registry
void  pl__set_data  (const char* pcName, void* pData);
void* pl__get_data  (const char* pcName);

// api registry
static void  pl__add_api        (const char* pcName, void* pInterface);
static void  pl__remove_api     (void* pInterface);
static void* pl__first_api      (const char* pcName);
static void* pl__next_api       (void* pPrev);
static void  pl__replace_api    (void* pOldInterface, void* pNewInterface);
static void  pl__subscribe_api  (void* pOldInterface, ptApiUpdateCallback ptCallback, void* pUserData);

// extension registry
static void pl__load_extension          (plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension);
static void pl__unload_extension        (plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension);
static void pl__handle_extension_reloads(plApiRegistryApiI* ptApiRegistry, plLibraryApiI* ptLibraryApi);

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// data registry
plHashMap    gtHashMap = {0};
plDataEntry* gsbDataEntries = NULL;

// api registry
plApiEntry*  gsbApiEntries = NULL;

// extension registry
plExtension*      gsbtExtensions  = NULL;
plSharedLibrary*  gsbtLibs  = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plApiRegistryApiI*
pl_load_api_registry(void)
{
    static plApiRegistryApiI tApiRegistry = {
        .add         = pl__add_api,
        .remove      = pl__remove_api,
        .first       = pl__first_api,
        .next        = pl__next_api,
        .replace     = pl__replace_api,
        .subscribe   = pl__subscribe_api
    };
    return &tApiRegistry;
}

void
pl_unload_api_registry(void)
{
    pl_sb_free(gsbApiEntries);
    gsbApiEntries = NULL;
}

void
pl_load_data_registry_api(plApiRegistryApiI* ptApiRegistry)
{
    static plDataRegistryApiI tApi = {
        .set_data = pl__set_data,
        .get_data = pl__get_data
    };
    ptApiRegistry->add(PL_API_DATA_REGISTRY, &tApi);
}

void
pl_unload_data_registry_api(void)
{
    pl_sb_free(gsbDataEntries);
    pl_hm_free(&gtHashMap);
}

void
pl_load_extension_registry(plApiRegistryApiI* ptApiRegistry)
{
    static plExtensionRegistryApiI tApi = {
        .load   = pl__load_extension,
        .unload = pl__unload_extension,
        .reload = pl__handle_extension_reloads
    };
    ptApiRegistry->add(PL_API_EXTENSION_REGISTRY, &tApi);
}

void
pl_unload_extension_registry(void)
{
    pl_sb_free(gsbtExtensions);
    pl_sb_free(gsbtLibs);
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

void
pl__set_data(const char* pcName, void* pData)
{
    const uint64_t ulHash = pl_hm_hash_str(pcName);

    const bool bDataExists = pl_hm_has_key(&gtHashMap, ulHash);

    if(!bDataExists)
    {
        uint64_t ulFreeIndex = pl_hm_get_free_index(&gtHashMap);
        if(ulFreeIndex == UINT64_MAX)
        {
            pl_sb_add(gsbDataEntries);
            ulFreeIndex = pl_sb_size(gsbDataEntries) - 1;
        }

        pl_hm_insert(&gtHashMap, ulHash, ulFreeIndex);

        gsbDataEntries[ulFreeIndex].pcName = pcName;
        gsbDataEntries[ulFreeIndex].pData = pData;

    }
}

void*
pl__get_data(const char* pcName)
{
    const uint64_t ulIndex = pl_hm_lookup_str(&gtHashMap, pcName);

    if(ulIndex == UINT64_MAX)
        return NULL;

    return gsbDataEntries[ulIndex].pData;
}

static void
pl__add_api(const char* pcName, void* pInterface)
{
    plApiEntry tNewApiEntry = {
        .pcName = pcName,
        .pInterface = pInterface
    };
    pl_sb_push(gsbApiEntries, tNewApiEntry);
}

static void
pl__remove_api(void* pInterface)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbApiEntries); i++)
    {
        if(gsbApiEntries[i].pInterface == pInterface)
        {
            pl_sb_free(gsbApiEntries[i].sbSubscribers);
            pl_sb_del_swap(gsbApiEntries, i);
            break;
        }
    }
}

static void
pl__replace_api(void* pOldInterface, void* pNewInterface)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbApiEntries); i++)
    {
        if(gsbApiEntries[i].pInterface == pOldInterface)
        {
            gsbApiEntries[i].pInterface = pNewInterface;

            for(uint32_t j = 0; j < pl_sb_size(gsbApiEntries[i].sbSubscribers); j++)
            {
                gsbApiEntries[i].sbSubscribers[j](pOldInterface, gsbApiEntries[i].sbUserData[j]);
            }
            pl_sb_reset(gsbApiEntries[i].sbSubscribers);
            break;
        }
    }
}

static void
pl__subscribe_api(void* pInterface, ptApiUpdateCallback ptCallback, void* pUserData)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbApiEntries); i++)
    {
        if(gsbApiEntries[i].pInterface == pInterface)
        {
            pl_sb_push(gsbApiEntries[i].sbSubscribers, ptCallback);
            pl_sb_push(gsbApiEntries[i].sbUserData, pUserData);
            break;
        }
    }
}

static void*
pl__first_api(const char* pcName)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbApiEntries); i++)
    {
        if(strcmp(pcName, gsbApiEntries[i].pcName) == 0)
        {
            return gsbApiEntries[i].pInterface;
        }
    }

    return NULL;
}

static void*
pl__next_api(void* pPrev)
{
    const char* pcName = "";
    for(uint32_t i = 0; i < pl_sb_size(gsbApiEntries); i++)
    {
        if(strcmp(pcName, gsbApiEntries[i].pcName) == 0)
        {
            return gsbApiEntries[i].pInterface;
        }

        if(gsbApiEntries[i].pInterface == pPrev)
        {
            pcName = gsbApiEntries[i].pcName;
        }
    }

    return NULL;
}

static void
pl__load_extension(plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension)
{

    // check if extension exists already
    for(uint32_t i = 0; i < pl_sb_size(gsbtLibs); i++)
    {
        if(strcmp(ptExtension->pcLibName, gsbtLibs[i].acPath) == 0)
            return;
    }

    plSharedLibrary tLibrary = {0};

    plLibraryApiI* ptLibraryApi = ptApiRegistry->first(PL_API_LIBRARY);

    if(ptLibraryApi->load_library(&tLibrary, ptExtension->pcLibName, ptExtension->pcTransName, "./lock.tmp"))
    {
        #ifdef _WIN32
            ptExtension->pl_load   = (void (__cdecl *)(plApiRegistryApiI*,  plExtension*, bool)) ptLibraryApi->load_library_function(&tLibrary, ptExtension->pcLoadFunc);
            ptExtension->pl_unload = (void (__cdecl *)(plApiRegistryApiI*, plExtension*))        ptLibraryApi->load_library_function(&tLibrary, ptExtension->pcUnloadFunc);
        #else // linux & apple
            ptExtension->pl_load   = (void (__attribute__(()) *)(plApiRegistryApiI*, plExtension*, bool)) ptLibraryApi->load_library_function(&tLibrary, ptExtension->pcLoadFunc);
            ptExtension->pl_unload = (void (__attribute__(()) *)(plApiRegistryApiI*, plExtension*))       ptLibraryApi->load_library_function(&tLibrary, ptExtension->pcUnloadFunc);
        #endif

        PL_ASSERT(ptExtension->pl_load);
        PL_ASSERT(ptExtension->pl_unload);
        pl_sb_push(gsbtLibs, tLibrary);
        ptExtension->pl_load(ptApiRegistry, ptExtension, false);
        pl_sb_push(gsbtExtensions, *ptExtension);
    }
    else
    {
        PL_ASSERT(false && "extension not loaded");
    }
}

static void
pl__unload_extension(plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbtExtensions); i++)
    {
        if(strcmp(ptExtension->pcLibName, gsbtExtensions[i].pcLibName) == 0)
        {
            gsbtExtensions[i].pl_unload(ptApiRegistry, ptExtension);
            pl_sb_del_swap(gsbtExtensions, i);
            pl_sb_del_swap(gsbtLibs, i);
            return;
        }
    }

    PL_ASSERT(false && "extension not found");
}

static void
pl__handle_extension_reloads(plApiRegistryApiI* ptApiRegistry, plLibraryApiI* ptLibraryApi)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbtLibs); i++)
    {
        if(ptLibraryApi->has_library_changed(&gsbtLibs[i]))
        {
            plSharedLibrary* ptLibrary = &gsbtLibs[i];
            ptLibraryApi->reload_library(ptLibrary);
            plExtension* ptExtension = &gsbtExtensions[i];
                #ifdef _WIN32
                    ptExtension->pl_load   = (void (__cdecl *)(plApiRegistryApiI*, plExtension*, bool)) ptLibraryApi->load_library_function(ptLibrary, ptExtension->pcLoadFunc);
                    ptExtension->pl_unload = (void (__cdecl *)(plApiRegistryApiI*, plExtension*))       ptLibraryApi->load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
                #else // linux & apple
                    ptExtension->pl_load   = (void (__attribute__(()) *)(plApiRegistryApiI*, plExtension*, bool)) ptLibraryApi->load_library_function(ptLibrary, ptExtension->pcLoadFunc);
                    ptExtension->pl_unload = (void (__attribute__(()) *)(plApiRegistryApiI*, plExtension*))       ptLibraryApi->load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
                #endif

                PL_ASSERT(ptExtension->pl_load);
                PL_ASSERT(ptExtension->pl_unload);

                ptExtension->pl_load(ptApiRegistry, ptExtension, true);
        }
            
    }
}