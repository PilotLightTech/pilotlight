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

typedef struct _plApiEntry
{
    const char*          pcName;
    const void*          pInterface;
    ptApiUpdateCallback* sbSubscribers;
    void**               sbUserData;
} plApiEntry;

typedef struct _plDataRegistryData
{
    plDataObject** sbtDataObjects;
    plDataObject** sbtDataObjectsDeletionQueue;
    plDataID*      sbtFreeDataIDs;
    plDataObject*  aptObjects[1024];
} plDataRegistryData;

typedef union _plDataObjectProperty
{
    const char* pcValue;
    void*       pValue;
} plDataObjectProperty;

typedef struct _plDataObject
{
    plDataID              tId;
    uint32_t              uReferenceCount;
    plDataObjectProperty  atDefaultProperties[2];
    uint32_t              uPropertyCount;
    plDataObjectProperty* ptProperties;
} plDataObject;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// data registry functions
void  pl__set_data(const char* pcName, void* pData);
void* pl__get_data(const char* pcName);

// new data registry functions

void                pl__garbage_collect(void);
plDataID            pl__create_object(void);
plDataID            pl__get_object_by_name(const char* pcName);
const plDataObject* pl__read      (plDataID);
void                pl__end_read  (const plDataObject* ptReader);
const char*         pl__get_string(const plDataObject*, uint32_t uProperty);
void*               pl__get_buffer(const plDataObject*, uint32_t uProperty);
plDataObject*       pl__write     (plDataID);
void                pl__set_string(plDataObject*, uint32_t, const char*);
void                pl__set_buffer(plDataObject*, uint32_t, void*);
void                pl__commit    (plDataObject*);

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
plDataRegistryData gtDataRegistryData = {0};
plMutex*     gptDataMutex = NULL;

// api registry
plApiEntry* gsbApiEntries = NULL;

// extension registry
plExtension*      gsbtExtensions  = NULL;
plSharedLibrary** gsbptLibs        = NULL;
uint32_t*         gsbtHotLibs     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plApiRegistryI*
pl_load_core_apis(void)
{

    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();
    pl__create_mutex(&gptDataMutex);

    pl_sb_resize(gtDataRegistryData.sbtFreeDataIDs, 1024);
    for(uint32_t i = 0; i < 1024; i++)
    {
        gtDataRegistryData.sbtFreeDataIDs[i].uIndex = i;
    }

    static const plDataRegistryI tApi0 = {
        .set_data = pl__set_data,
        .get_data = pl__get_data,
        .garbage_collect = pl__garbage_collect,
        .create_object = pl__create_object,
        .get_object_by_name = pl__get_object_by_name,
        .read = pl__read,
        .end_read = pl__end_read,
        .get_string = pl__get_string,
        .get_buffer = pl__get_buffer,
        .write = pl__write,
        .set_string = pl__set_string,
        .set_buffer = pl__set_buffer,
        .commit = pl__commit
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

    pl_sb_free(gsbtExtensions);
    pl_sb_free(gsbptLibs);
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
    plDataID tData = {
        .ulData = pl_hm_lookup_str(&gtHashMap, pcName)
    };

    if(tData.ulData == UINT64_MAX)
    {
        tData = pl__create_object();
    }
    plDataObject* ptWriter = pl__write(tData);
    pl__set_string(ptWriter, 0, pcName);
    pl__set_buffer(ptWriter, 1, pData);
    pl__commit(ptWriter);
}

void*
pl__get_data(const char* pcName)
{
    plDataID tData = pl__get_object_by_name(pcName);
    const plDataObject* ptReader = pl__read(tData);
    void* pData = pl__get_buffer(ptReader, 1);
    pl__end_read(ptReader);
    return pData;
}

void
pl__garbage_collect(void)
{
    pl__lock_mutex(gptDataMutex);
    for(uint32_t i = 0; i < pl_sb_size(gtDataRegistryData.sbtDataObjectsDeletionQueue); i++)
    {
        if(gtDataRegistryData.sbtDataObjectsDeletionQueue[i]->uReferenceCount == 0)
        {
            pl_sb_push(gtDataRegistryData.sbtDataObjects, gtDataRegistryData.sbtDataObjectsDeletionQueue[i]);
            pl_sb_del_swap(gtDataRegistryData.sbtDataObjectsDeletionQueue, i);
            i--;
        }
    }
    pl__unlock_mutex(gptDataMutex);
}

plDataID
pl__create_object(void)
{
    plDataID tId = {.ulData = UINT64_MAX};

    pl__lock_mutex(gptDataMutex);
    if(pl_sb_size(gtDataRegistryData.sbtFreeDataIDs) > 0)
    {
        tId = pl_sb_pop(gtDataRegistryData.sbtFreeDataIDs);
    }
    else
    {
        PL_ASSERT(false);
    }

    plDataObject* ptObject = NULL;
    if(pl_sb_size(gtDataRegistryData.sbtDataObjects) > 0)
    {
        ptObject = pl_sb_pop(gtDataRegistryData.sbtDataObjects);
    }
    else
    {
        ptObject = PL_ALLOC(sizeof(plDataObject));
        memset(ptObject, 0, sizeof(plDataObject));
    }
    pl__unlock_mutex(gptDataMutex);
    ptObject->tId = tId;

    ptObject->uPropertyCount = 2;
    ptObject->ptProperties = ptObject->atDefaultProperties;
    ptObject->atDefaultProperties[0].pcValue = NULL;
    ptObject->atDefaultProperties[1].pValue = NULL;

    gtDataRegistryData.aptObjects[tId.uIndex] = ptObject;

    return tId;
}

plDataID
pl__get_object_by_name(const char* pcName)
{
    plDataID tID = {
        .ulData = pl_hm_lookup_str(&gtHashMap, pcName)
    };
    return tID;
}

const plDataObject*
pl__read(plDataID tId)
{
    gtDataRegistryData.aptObjects[tId.uIndex]->uReferenceCount++;
    return gtDataRegistryData.aptObjects[tId.uIndex];
}

void
pl__end_read(const plDataObject* ptReader)
{
    gtDataRegistryData.aptObjects[ptReader->tId.uIndex]->uReferenceCount--;
}

const char*
pl__get_string(const plDataObject* ptReader, uint32_t uProperty)
{
    return ptReader->ptProperties[uProperty].pcValue;
}

void*
pl__get_buffer(const plDataObject* ptReader, uint32_t uProperty)
{
    return ptReader->ptProperties[uProperty].pValue;
}

plDataObject*
pl__write(plDataID tId)
{
    const plDataObject* ptOriginalObject = gtDataRegistryData.aptObjects[tId.uIndex];

    pl__lock_mutex(gptDataMutex);
    plDataObject* ptObject = NULL;
    if(pl_sb_size(gtDataRegistryData.sbtDataObjects) > 0)
    {
        ptObject = pl_sb_pop(gtDataRegistryData.sbtDataObjects);
    }
    else
    {
        ptObject = PL_ALLOC(sizeof(plDataObject));
        memset(ptObject, 0, sizeof(plDataObject));
    }
    pl__unlock_mutex(gptDataMutex);

    memcpy(ptObject, ptOriginalObject, sizeof(plDataObject));
    ptObject->uReferenceCount = 0;
    ptObject->ptProperties = ptObject->atDefaultProperties;

    return ptObject;
}

void
pl__set_string(plDataObject* ptWriter, uint32_t uProperty, const char* pcValue)
{
    ptWriter->ptProperties[uProperty].pcValue = pcValue;
    if(uProperty == 0)
    {
        if(pl_hm_has_key_str(&gtHashMap, pcValue))
        {
            pl_hm_remove_str(&gtHashMap, pcValue);
        }
        else
        {
            pl_hm_insert_str(&gtHashMap, pcValue, ptWriter->tId.ulData);
        }
    }
}

void
pl__set_buffer(plDataObject* ptWriter, uint32_t uProperty, void* pData)
{
    ptWriter->ptProperties[uProperty].pValue = pData;
}

void
pl__commit(plDataObject* ptWriter)
{
    plDataObject* ptOriginalObject = gtDataRegistryData.aptObjects[ptWriter->tId.uIndex];
    pl__lock_mutex(gptDataMutex);
    pl_sb_push(gtDataRegistryData.sbtDataObjectsDeletionQueue, ptOriginalObject);
    pl__unlock_mutex(gptDataMutex);
    gtDataRegistryData.aptObjects[ptWriter->tId.uIndex] = ptWriter;
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
    for(uint32_t i = 0; i < pl_sb_size(gsbptLibs); i++)
    {
        if(strcmp(tExtension.pcLibPath, gsbptLibs[i]->acPath) == 0)
            return;
    }

    plSharedLibrary* ptLibrary = NULL;

    const plLibraryApiI* ptLibraryApi = ptApiRegistry->first(PL_API_LIBRARY);

    if(ptLibraryApi->load(tExtension.pcLibPath, tExtension.pcTransName, "./lock.tmp", &ptLibrary))
    {
        #ifdef _WIN32
            tExtension.pl_load   = (void (__cdecl *)(const plApiRegistryI*, bool))  ptLibraryApi->load_function(ptLibrary, tExtension.pcLoadFunc);
            tExtension.pl_unload = (void (__cdecl *)(const plApiRegistryI*))        ptLibraryApi->load_function(ptLibrary, tExtension.pcUnloadFunc);
        #else // linux & apple
            tExtension.pl_load   = (void (__attribute__(()) *)(const plApiRegistryI*, bool)) ptLibraryApi->load_function(ptLibrary, tExtension.pcLoadFunc);
            tExtension.pl_unload = (void (__attribute__(()) *)(const plApiRegistryI*))       ptLibraryApi->load_function(ptLibrary, tExtension.pcUnloadFunc);
        #endif

        PL_ASSERT(tExtension.pl_load);
        PL_ASSERT(tExtension.pl_unload);
        pl_sb_push(gsbptLibs, ptLibrary);
        if(bReloadable)
            pl_sb_push(gsbtHotLibs, pl_sb_size(gsbptLibs) - 1);
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
            PL_FREE(gsbptLibs[i]);
            gsbptLibs[i] = NULL;
            pl_sb_del_swap(gsbtExtensions, i);
            pl_sb_del_swap(gsbptLibs, i);
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
        if(pl__has_library_changed(gsbptLibs[gsbtHotLibs[i]]))
        {
            plSharedLibrary* ptLibrary = gsbptLibs[gsbtHotLibs[i]];
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

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

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

#ifdef PL_USE_UI
#include "pl_ui.c"
#include "pl_ui_widgets.c"
#include "pl_ui_draw.c"
#endif