/*
    pilotlight_exe.c
*/

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

#include <stdbool.h> // bool
#include <string.h>  // strcmp
#include "pilotlight.h"
#include "pl_json.h"
#include "pl_ds.h"
#include "pl_memory.h"
#include "pl_os.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plExtension
{
    char pcLibName[128];
    char pcLibPath[128];
    char pcTransName[128];
    char pcLoadFunc[128];
    char pcUnloadFunc[128];

    void (*pl_load)   (const plApiRegistryI* ptApiRegistry, bool bReload);
    void (*pl_unload) (const plApiRegistryI* ptApiRegistry);
} plExtension;

typedef struct _plDataEntry
{
    const char* pcName;
    void*       pData;
} plDataEntry;

typedef struct _plApiEntry
{
    const char*          pcName;
    const void*          pInterface;
    ptApiUpdateCallback* sbSubscribers;
    void**               sbUserData;
} plApiEntry;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// data registry functions
void  pl__set_data(const char* pcName, void* pData);
void* pl__get_data(const char* pcName);

// api registry functions
static const void* pl__add_api        (const char* pcName, const void* pInterface);
static       void  pl__remove_api     (const void* pInterface);
static const void* pl__first_api      (const char* pcName);
static const void* pl__next_api       (const void* pPrev);
static       void  pl__replace_api    (const void* pOldInterface, const void* pNewInterface);
static       void  pl__subscribe_api  (const void* pOldInterface, ptApiUpdateCallback ptCallback, void* pUserData);

// extension registry functions
static void pl__load_extension             (const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable);
static void pl__unload_extension           (const char* pcName);
static void pl__unload_all_extensions      (void);
static void pl__handle_extension_reloads   (void);

// extension registry helper functions
static void pl__create_extension(const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, plExtension* ptExtensionOut);

static const plApiRegistryI*
pl__load_api_registry(void)
{
    static const plApiRegistryI tApiRegistry = {
        .add         = pl__add_api,
        .remove      = pl__remove_api,
        .first       = pl__first_api,
        .next        = pl__next_api,
        .replace     = pl__replace_api,
        .subscribe   = pl__subscribe_api
    };

    return &tApiRegistry;
}

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// data registry
plHashMap    gtHashMap = {0};
plDataEntry* gsbDataEntries = NULL;

// api registry
plApiEntry* gsbApiEntries = NULL;

// extension registry
plExtension*      gsbtExtensions  = NULL;
plSharedLibrary*  gsbtLibs        = NULL;
uint32_t*         gsbtHotLibs     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plApiRegistryI*
pl_load_core_apis(void)
{

    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    static const plDataRegistryI tApi0 = {
        .set_data = pl__set_data,
        .get_data = pl__get_data
    };

    static const plExtensionRegistryI tApi1 = {
        .load       = pl__load_extension,
        .unload     = pl__unload_extension,
        .unload_all = pl__unload_all_extensions,
        .reload     = pl__handle_extension_reloads
    };

    // apis more likely to not be stored, should be first (api registry is not sorted)
    ptApiRegistry->add(PL_API_DATA_REGISTRY, &tApi0);
    ptApiRegistry->add(PL_API_EXTENSION_REGISTRY, &tApi1);

    return ptApiRegistry;
}

void
pl_unload_core_apis(void)
{
    const uint32_t uApiCount = pl_sb_size(gsbApiEntries);
    for(uint32_t i = 0; i < uApiCount; i++)
    {
        pl_sb_free(gsbApiEntries[i].sbSubscribers);
        pl_sb_free(gsbApiEntries[i].sbUserData);
    }

    pl_sb_free(gsbDataEntries);
    pl_sb_free(gsbtExtensions);
    pl_sb_free(gsbtLibs);
    pl_sb_free(gsbtHotLibs);
    pl_sb_free(gsbApiEntries);
    pl_hm_free(&gtHashMap);
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

static const void*
pl__add_api(const char* pcName, const void* pInterface)
{
    plApiEntry tNewApiEntry = {
        .pcName = pcName,
        .pInterface = pInterface
    };
    pl_sb_push(gsbApiEntries, tNewApiEntry);
    return pInterface;
}

static void
pl__remove_api(const void* pInterface)
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
pl__replace_api(const void* pOldInterface, const void* pNewInterface)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbApiEntries); i++)
    {
        if(gsbApiEntries[i].pInterface == pOldInterface)
        {
            gsbApiEntries[i].pInterface = pNewInterface;

            for(uint32_t j = 0; j < pl_sb_size(gsbApiEntries[i].sbSubscribers); j++)
            {
                gsbApiEntries[i].sbSubscribers[j](pNewInterface, pOldInterface, gsbApiEntries[i].sbUserData[j]);
            }
            pl_sb_reset(gsbApiEntries[i].sbSubscribers);
            break;
        }
    }
}

static void
pl__subscribe_api(const void* pInterface, ptApiUpdateCallback ptCallback, void* pUserData)
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

static const void*
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

static const void*
pl__next_api(const void* pPrev)
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
pl__create_extension(const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, plExtension* ptExtensionOut)
{

    #ifdef _WIN32
        pl_sprintf(ptExtensionOut->pcLibPath, "./%s.dll", pcName);
    #elif defined(__APPLE__)
        pl_sprintf(ptExtensionOut->pcLibPath, "./%s.dylib", pcName);
    #else
        pl_sprintf(ptExtensionOut->pcLibPath, "./%s.so", pcName);
    #endif
    strcpy(ptExtensionOut->pcLibName, pcName);
    strcpy(ptExtensionOut->pcLoadFunc, pcLoadFunc);
    strcpy(ptExtensionOut->pcUnloadFunc, pcUnloadFunc);
    pl_sprintf(ptExtensionOut->pcTransName, "./%s_", pcName); 
}

static void
pl__load_extension(const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable)
{

    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    plExtension tExtension = {0};
    pl__create_extension(pcName, pcLoadFunc, pcUnloadFunc, &tExtension);

    // check if extension exists already
    for(uint32_t i = 0; i < pl_sb_size(gsbtLibs); i++)
    {
        if(strcmp(tExtension.pcLibPath, gsbtLibs[i].acPath) == 0)
            return;
    }

    plSharedLibrary tLibrary = {0};

    const plLibraryApiI* ptLibraryApi = ptApiRegistry->first(PL_API_LIBRARY);

    if(ptLibraryApi->load(&tLibrary, tExtension.pcLibPath, tExtension.pcTransName, "./lock.tmp"))
    {
        #ifdef _WIN32
            tExtension.pl_load   = (void (__cdecl *)(const plApiRegistryI*, bool))  ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
            tExtension.pl_unload = (void (__cdecl *)(const plApiRegistryI*))        ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
        #else // linux & apple
            tExtension.pl_load   = (void (__attribute__(()) *)(const plApiRegistryI*, bool)) ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
            tExtension.pl_unload = (void (__attribute__(()) *)(const plApiRegistryI*))       ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
        #endif

        PL_ASSERT(tExtension.pl_load);
        PL_ASSERT(tExtension.pl_unload);
        pl_sb_push(gsbtLibs, tLibrary);
        if(bReloadable)
            pl_sb_push(gsbtHotLibs, pl_sb_size(gsbtLibs) - 1);
        tExtension.pl_load(ptApiRegistry, false);
        pl_sb_push(gsbtExtensions, tExtension);
    }
    else
    {
        printf("Extension: %s not loaded\n", tExtension.pcLibPath);
        PL_ASSERT(false && "extension not loaded");
    }
}

static void
pl__unload_extension(const char* pcName)
{
    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    for(uint32_t i = 0; i < pl_sb_size(gsbtExtensions); i++)
    {
        if(strcmp(pcName, gsbtExtensions[i].pcLibName) == 0)
        {
            gsbtExtensions[i].pl_unload(ptApiRegistry);
            pl_sb_del_swap(gsbtExtensions, i);
            pl_sb_del_swap(gsbtLibs, i);
            pl_sb_del_swap(gsbtHotLibs, i);
            return;
        }
    }

    PL_ASSERT(false && "extension not found");
}

static void
pl__unload_all_extensions(void)
{
    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    for(uint32_t i = 0; i < pl_sb_size(gsbtExtensions); i++)
    {
        if(gsbtExtensions[i].pl_unload)
            gsbtExtensions[i].pl_unload(ptApiRegistry);
    }
}

static void
pl__handle_extension_reloads(void)
{
    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    for(uint32_t i = 0; i < pl_sb_size(gsbtHotLibs); i++)
    {
        if(pl__has_library_changed(&gsbtLibs[gsbtHotLibs[i]]))
        {
            plSharedLibrary* ptLibrary = &gsbtLibs[gsbtHotLibs[i]];
            plExtension* ptExtension = &gsbtExtensions[gsbtHotLibs[i]];
            ptExtension->pl_unload(ptApiRegistry);
            pl__reload_library(ptLibrary); 
            #ifdef _WIN32
                ptExtension->pl_load   = (void (__cdecl *)(const plApiRegistryI*, bool)) pl__load_library_function(ptLibrary, ptExtension->pcLoadFunc);
                ptExtension->pl_unload = (void (__cdecl *)(const plApiRegistryI*))       pl__load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
            #else // linux & apple
                ptExtension->pl_load   = (void (__attribute__(()) *)(const plApiRegistryI*, bool)) pl__load_library_function(ptLibrary, ptExtension->pcLoadFunc);
                ptExtension->pl_unload = (void (__attribute__(()) *)(const plApiRegistryI*))       pl__load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
            #endif
            PL_ASSERT(ptExtension->pl_load);
            PL_ASSERT(ptExtension->pl_unload);
            ptExtension->pl_load(ptApiRegistry, true);
        }
            
    }
}

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION

#define PL_MEMORY_IMPLEMENTATION
#include "pl_memory.h"
#undef PL_MEMORY_IMPLEMENTATION

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
#undef PL_STRING_IMPLEMENTATION

void*
pl_realloc(void* pBuffer, size_t szSize, const char* pcFile, int iLine)
{
    return realloc(pBuffer, szSize);
}

// ui
#include "pl_ui.c"
#include "pl_ui_widgets.c"
#include "pl_ui_draw.c"