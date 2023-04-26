/*
    pilotlight_exe.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] header mess
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

// memory functions (forward declared to be used in pl_ds.h)
static void* pl__alloc(size_t szSize);
static void  pl__free (void* pBuffer);

// pl_ds.h allocators (so they can be tracked)
#define PL_DS_ALLOC(x) pl__alloc((x))
#define PL_DS_FREE(x)  pl__free((x))

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <string.h>  // strcmp
#include "pilotlight.h"
#include "pl_json.h"
#include "pl_ds.h"

#ifdef _WIN32
    #include "../backends/pl_win32.c"
#elif defined(__APPLE__)
    #include "../backends/pl_macos.m"
#else
    #include "../backends/pl_linux.c"
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plExtension
{
    char pcLibName[128];
    char pcTransName[128];
    char pcLoadFunc[128];
    char pcUnloadFunc[128];

    void (*pl_load)   (plApiRegistryApiI* ptApiRegistry, bool bReload);
    void (*pl_unload) (plApiRegistryApiI* ptApiRegistry);
} plExtension;

typedef struct _plDataEntry
{
    const char* pcName;
    void*       pData;
} plDataEntry;

typedef struct _plApiEntry
{
    const char*          pcName;
    void*                pInterface;
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
static void  pl__add_api        (const char* pcName, void* pInterface);
static void  pl__remove_api     (void* pInterface);
static void* pl__first_api      (const char* pcName);
static void* pl__next_api       (void* pPrev);
static void  pl__replace_api    (void* pOldInterface, void* pNewInterface);
static void  pl__subscribe_api  (void* pOldInterface, ptApiUpdateCallback ptCallback, void* pUserData);

// extension registry functions
static void pl__load_extensions_from_config(plApiRegistryApiI* ptApiRegistry, const char* pcConfigFile);
static void pl__load_extensions_from_file  (plApiRegistryApiI* ptApiRegistry, const char* pcFile);
static void pl__load_extension             (plApiRegistryApiI* ptApiRegistry, const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc);
static void pl__unload_extension           (plApiRegistryApiI* ptApiRegistry, const char* pcName);
static void pl__handle_extension_reloads   (plApiRegistryApiI* ptApiRegistry);

// extension registry helper functions
static void pl__create_extension(const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, plExtension* ptExtensionOut);

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// data registry
plHashMap    gtHashMap = {0};
plDataEntry* gsbDataEntries = NULL;

// api registry
plApiEntry* gsbApiEntries = NULL;

// extension registry
plExtension*     gsbtExtensions  = NULL;
plSharedLibrary* gsbtLibs  = NULL;

// memory
size_t gszActiveAllocations = 0;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plApiRegistryApiI*
pl_load_core_apis(void)
{

    static plApiRegistryApiI tApiRegistry = {
        .add         = pl__add_api,
        .remove      = pl__remove_api,
        .first       = pl__first_api,
        .next        = pl__next_api,
        .replace     = pl__replace_api,
        .subscribe   = pl__subscribe_api
    };

    static plDataRegistryApiI tApi0 = {
        .set_data = pl__set_data,
        .get_data = pl__get_data
    };

    static plExtensionRegistryApiI tApi1 = {
        .load             = pl__load_extension,
        .unload           = pl__unload_extension,
        .reload           = pl__handle_extension_reloads,
        .load_from_config = pl__load_extensions_from_config,
        .load_from_file   = pl__load_extensions_from_file
    };

    static plMemoryApiI tApi2 = {
        .alloc = pl__alloc,
        .free  = pl__free
    };

    static plLibraryApiI tApi3 = {
        .has_changed   = pl__has_library_changed,
        .load          = pl__load_library,
        .load_function = pl__load_library_function,
        .reload        = pl__reload_library
    };

    tApiRegistry.add(PL_API_DATA_REGISTRY, &tApi0);
    tApiRegistry.add(PL_API_EXTENSION_REGISTRY, &tApi1);
    tApiRegistry.add(PL_API_MEMORY, &tApi2);
    tApiRegistry.add(PL_API_LIBRARY, &tApi3);

    return &tApiRegistry;
}

void
pl_unload_core_apis(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbApiEntries); i++)
    {
        pl_sb_free(gsbApiEntries[i].sbSubscribers);
        pl_sb_free(gsbApiEntries[i].sbUserData);
    }

    pl_sb_free(gsbDataEntries);
    pl_sb_free(gsbtExtensions);
    pl_sb_free(gsbtLibs);
    pl_sb_free(gsbApiEntries);
    pl_hm_free(&gtHashMap);

    PL_ASSERT(gszActiveAllocations == 0);
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
                gsbApiEntries[i].sbSubscribers[j](pNewInterface, pOldInterface, gsbApiEntries[i].sbUserData[j]);
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
pl__create_extension(const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, plExtension* ptExtensionOut)
{

    #ifdef _WIN32
        pl_sprintf(ptExtensionOut->pcLibName, "./%s.dll", pcName);
    #elif defined(__APPLE__)
        pl_sprintf(ptExtensionOut->pcLibName, "./%s.dylib", pcName);
    #else
        pl_sprintf(ptExtensionOut->pcLibName, "./%s.so", pcName);
    #endif
    strcpy(ptExtensionOut->pcLoadFunc, pcLoadFunc);
    strcpy(ptExtensionOut->pcUnloadFunc, pcUnloadFunc);
    pl_sprintf(ptExtensionOut->pcTransName, "./%s_", pcName); 
}

static void
pl__load_extensions_from_config(plApiRegistryApiI* ptApiRegistry, const char* pcConfigFile)
{

    plFileApiI* ptFileApi = ptApiRegistry->first(PL_API_FILE);

    unsigned int uFileSize = 0;
    ptFileApi->read(pcConfigFile, &uFileSize, NULL, "rb");
    char* pcBuffer = pl__alloc(uFileSize + 1);
    memset(pcBuffer, 0, uFileSize + 1);
    ptFileApi->read(pcConfigFile, &uFileSize, pcBuffer, "rb");

    plJsonObject tRootJsonObject = {0};
    pl_load_json(pcBuffer, &tRootJsonObject);

    if(!pl_json_member_exist(&tRootJsonObject, "extensions"))
    {
        pl_unload_json(&tRootJsonObject);
        pl__free(pcBuffer);
        return;
    }

    uint32_t uExtensionCount = 0;
    plJsonObject* sbtExtensions = pl_json_array_member(&tRootJsonObject, "extensions", &uExtensionCount);

    for(uint32_t uExtensionIndex = 0; uExtensionIndex < uExtensionCount; uExtensionIndex++)
    {

        plJsonObject* ptExtension = &sbtExtensions[uExtensionIndex];

        plExtension tExtension = {0};

        char acTextBuffer[512] = {0};

        // read from file if file member exists
        if(pl_json_member_exist(ptExtension, "file"))
        {
            pl_json_string_member(ptExtension, "file", acTextBuffer, 512);
            pl__load_extensions_from_file(ptApiRegistry, acTextBuffer);
        }
        else
        {
            PL_ASSERT(pl_json_member_exist(ptExtension, "name") && "extension config must have 'name'");
            PL_ASSERT(pl_json_member_exist(ptExtension, "load") && "extension config must have 'load'");
            PL_ASSERT(pl_json_member_exist(ptExtension, "unload") && "extension config must have 'unload'");

            pl_json_string_member(ptExtension, "name", acTextBuffer, 128);
            pl_json_string_member(ptExtension, "load", tExtension.pcLoadFunc, 128);
            pl_json_string_member(ptExtension, "unload", tExtension.pcUnloadFunc, 128);
            pl_sprintf(tExtension.pcTransName, "./%s_", acTextBuffer);

            #ifdef _WIN32
                pl_sprintf(tExtension.pcLibName, "./%s.dll", acTextBuffer);
            #elif defined(__APPLE__)
                pl_sprintf(tExtension.pcLibName, "./%s.dylib", acTextBuffer);
            #else
                pl_sprintf(tExtension.pcLibName, "./%s.so", acTextBuffer);
            #endif

            // check if extension exists already
            bool bExists = false;
            for(uint32_t i = 0; i < pl_sb_size(gsbtLibs); i++)
            {
                if(strcmp(tExtension.pcLibName, gsbtLibs[i].acPath) == 0)
                {
                    bExists = true;
                }
            }

            if(!bExists)
            {
                plSharedLibrary tLibrary = {0};

                plLibraryApiI* ptLibraryApi = ptApiRegistry->first(PL_API_LIBRARY);

                if(ptLibraryApi->load(&tLibrary, tExtension.pcLibName, tExtension.pcTransName, "./lock.tmp"))
                {
                    #ifdef _WIN32
                        tExtension.pl_load   = (void (__cdecl *)(plApiRegistryApiI*, bool)) ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
                        tExtension.pl_unload = (void (__cdecl *)(plApiRegistryApiI*))       ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
                    #else // linux & apple
                        tExtension.pl_load   = (void (__attribute__(()) *)(plApiRegistryApiI*, bool)) ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
                        tExtension.pl_unload = (void (__attribute__(()) *)(plApiRegistryApiI*))       ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
                    #endif

                    PL_ASSERT(tExtension.pl_load);
                    PL_ASSERT(tExtension.pl_unload);
                    pl_sb_push(gsbtLibs, tLibrary);
                    tExtension.pl_load(ptApiRegistry, false);
                    pl_sb_push(gsbtExtensions, tExtension);
                }
                else
                {
                    PL_ASSERT(false && "extension not loaded");
                }
            }
        }
    }

    pl_unload_json(&tRootJsonObject);
    pl__free(pcBuffer);
}

static void
pl__load_extensions_from_file(plApiRegistryApiI* ptApiRegistry, const char* pcFile)
{
    plFileApiI* ptFileApi = ptApiRegistry->first(PL_API_FILE);

    unsigned int uFileSize = 0;
    ptFileApi->read(pcFile, &uFileSize, NULL, "rb");
    char* pcBuffer = pl__alloc(uFileSize + 1);
    memset(pcBuffer, 0, uFileSize + 1);
    ptFileApi->read(pcFile, &uFileSize, pcBuffer, "rb");

    plJsonObject tRootJsonObject = {0};
    pl_load_json(pcBuffer, &tRootJsonObject);

    plExtension tExtension = {0};

    char acLibName[128] = {0};
    pl_json_string_member(&tRootJsonObject, "name", acLibName, 128);
    pl_json_string_member(&tRootJsonObject, "load", tExtension.pcLoadFunc, 128);
    pl_json_string_member(&tRootJsonObject, "unload", tExtension.pcUnloadFunc, 128);
    pl_sprintf(tExtension.pcTransName, "./%s_", acLibName);

    #ifdef _WIN32
        pl_sprintf(tExtension.pcLibName, "./%s.dll", acLibName);
    #elif defined(__APPLE__)
        pl_sprintf(tExtension.pcLibName, "./%s.dylib", acLibName);
    #else
        pl_sprintf(tExtension.pcLibName, "./%s.so", acLibName);
    #endif

    // check if extension exists already
    bool bExists = false;
    for(uint32_t i = 0; i < pl_sb_size(gsbtLibs); i++)
    {
        if(strcmp(tExtension.pcLibName, gsbtLibs[i].acPath) == 0)
        {
            bExists = true;
        }
    }

    if(!bExists)
    {
        plSharedLibrary tLibrary = {0};

        plLibraryApiI* ptLibraryApi = ptApiRegistry->first(PL_API_LIBRARY);

        if(ptLibraryApi->load(&tLibrary, tExtension.pcLibName, tExtension.pcTransName, "./lock.tmp"))
        {
            #ifdef _WIN32
                tExtension.pl_load   = (void (__cdecl *)(plApiRegistryApiI*, bool)) ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
                tExtension.pl_unload = (void (__cdecl *)(plApiRegistryApiI*))       ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
            #else // linux & apple
                tExtension.pl_load   = (void (__attribute__(()) *)(plApiRegistryApiI*, bool)) ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
                tExtension.pl_unload = (void (__attribute__(()) *)(plApiRegistryApiI*))       ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
            #endif

            PL_ASSERT(tExtension.pl_load);
            PL_ASSERT(tExtension.pl_unload);
            pl_sb_push(gsbtLibs, tLibrary);
            tExtension.pl_load(ptApiRegistry, false);
            pl_sb_push(gsbtExtensions, tExtension);
        }
        else
        {
            PL_ASSERT(false && "extension not loaded");
        }
    }
    pl_unload_json(&tRootJsonObject);
    pl__free(pcBuffer);
}

static void
pl__load_extension(plApiRegistryApiI* ptApiRegistry, const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc)
{

    plExtension tExtension = {0};
    pl__create_extension(pcName, pcLoadFunc, pcUnloadFunc, &tExtension);

    // check if extension exists already
    for(uint32_t i = 0; i < pl_sb_size(gsbtLibs); i++)
    {
        if(strcmp(tExtension.pcLibName, gsbtLibs[i].acPath) == 0)
            return;
    }

    plSharedLibrary tLibrary = {0};

    plLibraryApiI* ptLibraryApi = ptApiRegistry->first(PL_API_LIBRARY);

    if(ptLibraryApi->load(&tLibrary, tExtension.pcLibName, tExtension.pcTransName, "./lock.tmp"))
    {
        #ifdef _WIN32
            tExtension.pl_load   = (void (__cdecl *)(plApiRegistryApiI*, bool))  ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
            tExtension.pl_unload = (void (__cdecl *)(plApiRegistryApiI*))        ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
        #else // linux & apple
            tExtension.pl_load   = (void (__attribute__(()) *)(plApiRegistryApiI*, bool)) ptLibraryApi->load_function(&tLibrary, tExtension.pcLoadFunc);
            tExtension.pl_unload = (void (__attribute__(()) *)(plApiRegistryApiI*))       ptLibraryApi->load_function(&tLibrary, tExtension.pcUnloadFunc);
        #endif

        PL_ASSERT(tExtension.pl_load);
        PL_ASSERT(tExtension.pl_unload);
        pl_sb_push(gsbtLibs, tLibrary);
        tExtension.pl_load(ptApiRegistry, false);
        pl_sb_push(gsbtExtensions, tExtension);
    }
    else
    {
        PL_ASSERT(false && "extension not loaded");
    }
}

static void
pl__unload_extension(plApiRegistryApiI* ptApiRegistry, const char* pcName)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbtExtensions); i++)
    {
        if(strcmp(pcName, gsbtExtensions[i].pcLibName) == 0)
        {
            gsbtExtensions[i].pl_unload(ptApiRegistry);
            pl_sb_del_swap(gsbtExtensions, i);
            pl_sb_del_swap(gsbtLibs, i);
            return;
        }
    }

    PL_ASSERT(false && "extension not found");
}

static void
pl__handle_extension_reloads(plApiRegistryApiI* ptApiRegistry)
{
    for(uint32_t i = 0; i < pl_sb_size(gsbtLibs); i++)
    {
        if(pl__has_library_changed(&gsbtLibs[i]))
        {
            plSharedLibrary* ptLibrary = &gsbtLibs[i];
            pl__reload_library(ptLibrary);
            plExtension* ptExtension = &gsbtExtensions[i];
                #ifdef _WIN32
                    ptExtension->pl_load   = (void (__cdecl *)(plApiRegistryApiI*, bool)) pl__load_library_function(ptLibrary, ptExtension->pcLoadFunc);
                    ptExtension->pl_unload = (void (__cdecl *)(plApiRegistryApiI*))       pl__load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
                #else // linux & apple
                    ptExtension->pl_load   = (void (__attribute__(()) *)(plApiRegistryApiI*, bool)) pl__load_library_function(ptLibrary, ptExtension->pcLoadFunc);
                    ptExtension->pl_unload = (void (__attribute__(()) *)(plApiRegistryApiI*))       pl__load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
                #endif

                PL_ASSERT(ptExtension->pl_load);
                PL_ASSERT(ptExtension->pl_unload);

                ptExtension->pl_load(ptApiRegistry, true);
        }
            
    }
}

static void*
pl__alloc(size_t szSize)
{
    gszActiveAllocations++;
    return malloc(szSize);
}

static void
pl__free(void* pBuffer)
{
    gszActiveAllocations--;
    free(pBuffer);
}

#define PL_IO_IMPLEMENTATION
#include "pl_io.h"
#undef PL_IO_IMPLEMENTATION

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"
#undef PL_JSON_IMPLEMENTATION