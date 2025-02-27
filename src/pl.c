/*
    pl.c
      - common definitions needed by platform backends
      - should be included near the end of a platform backend file as
        a unity build
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal structs
// [SECTION] enums
// [SECTION] public api
// [SECTION] global data
// [SECTION] api registry implementation
// [SECTION] data registry implementation
// [SECTION] extension registry implementation
// [SECTION] io implementation
// [SECTION] memory api implementation
// [SECTION] helper implementations
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h>   // FLT_MAX
#include <stdbool.h> // bool
#include <string.h>  // strcmp
#include "pl_internal.h"
#include "pl_ds.h"
#include "pl_memory.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// embedded extensions
#include "pl_window_ext.h"
#include "pl_library_ext.h"
#include "pl_file_ext.h"
#include "pl_atomics_ext.h"
#include "pl_threads_ext.h"
#include "pl_network_ext.h"
#include "pl_virtual_memory_ext.h"
#include "pl_profile_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_VEC2_LENGTH_SQR(vec) (((vec).x * (vec).x) + ((vec).y * (vec).y))

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plExtension
{
    char pcLibName[128];
    char pcLibPath[128];
    char pcLoadFunc[128];
    char pcUnloadFunc[128];
    bool bReloadable;

    void (*pl_load)   (const plApiRegistryI* ptApiRegistry, bool bReload);
    void (*pl_unload) (const plApiRegistryI* ptApiRegistry, bool bReload);
} plExtension;

typedef struct _plApiEntry plApiEntry;
typedef struct _plApiEntry
{
    const char* pcName;
    void*       apBuffer[PL_MAX_API_FUNCTIONS];
    plVersion   tVersion;
    plApiEntry* ptNext;
    bool        bSet;
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

typedef struct _plInputEvent
{
    plInputEventType   tType;
    plInputEventSource tSource;

    union
    {
        struct // mouse pos event
        {
            float fPosX;
            float fPosY;
        };

        struct // mouse wheel event
        {
            float fWheelX;
            float fWheelY;
        };
        
        struct // mouse button event
        {
            int  iButton;
            bool bMouseDown;
        };

        struct // key event
        {
            plKey tKey;
            bool  bKeyDown;
        };

        struct // text event
        {
            uint32_t uChar;
        };
        
    };

} plInputEvent;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

typedef enum
{
    PL_INPUT_EVENT_TYPE_NONE = 0,
    PL_INPUT_EVENT_TYPE_MOUSE_POS,
    PL_INPUT_EVENT_TYPE_MOUSE_WHEEL,
    PL_INPUT_EVENT_TYPE_MOUSE_BUTTON,
    PL_INPUT_EVENT_TYPE_KEY,
    PL_INPUT_EVENT_TYPE_TEXT,
    
    PL_INPUT_EVENT_TYPE_COUNT
} _plInputEventType;

typedef enum
{
    PL_INPUT_EVENT_SOURCE_NONE = 0,
    PL_INPUT_EVENT_SOURCE_MOUSE,
    PL_INPUT_EVENT_SOURCE_KEYBOARD,
    
    PL_INPUT_EVENT_SOURCE_COUNT
} _plInputEventSource;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// data registry functions
void  pl_set_data(const char* pcName, void* pData);
void* pl_get_data(const char* pcName);

// new data registry functions
plDataID            pl_create_object(void);
plDataID            pl_get_object_by_name(const char* pcName);
const plDataObject* pl_read      (plDataID);
void                pl_end_read  (const plDataObject* ptReader);
const char*         pl_get_string(const plDataObject*, uint32_t uProperty);
void*               pl_get_buffer(const plDataObject*, uint32_t uProperty);
plDataObject*       pl_write     (plDataID);
void                pl_set_string(plDataObject*, uint32_t, const char*);
void                pl_set_buffer(plDataObject*, uint32_t, void*);
void                pl_commit    (plDataObject*);

// api registry functions
void        pl__set_api(const char* pcName, plVersion, const void* pInterface, size_t szInterfaceSize);
const void* pl__get_api(const char* pcName, plVersion);
void        pl__remove_api(const void* pInterface);

// extension registry functions
bool pl_load_extension  (const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable);
bool pl_unload_extension(const char* pcName);

// IO helper functions
static void          pl__update_events(void);
static void          pl__update_mouse_inputs(void);
static void          pl__update_keyboard_inputs(void);
static int           pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate);
static plInputEvent* pl__get_last_event(plInputEventType tType, int iButtonOrKey);

const plApiRegistryI* pl__load_api_registry(void);

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

// logging
uint64_t guRuntimeLogChannel = 0;

// data registry
plHashMap*         gptHashmap = NULL;
plDataRegistryData gtDataRegistryData = {0};
plMutex*           gptDataMutex = {0};
plMutex*           gptMemoryMutex = {0};

// api registry
plApiEntry* gptApiHead = NULL;

// extension registry
plExtension*      gsbtExtensions = NULL;
plSharedLibrary** gsbptLibs      = NULL;
uint32_t*         gsbtHotLibs    = NULL; // index into gsbptLibs

// IO
plIO gtIO = {
    .fHeadlessUpdateRate      = 30.0f,
    .fMouseDoubleClickTime    = 0.3f,
    .fMouseDoubleClickMaxDist = 6.0f,
    .fMouseDragThreshold      = 6.0f,
    .fKeyRepeatDelay          = 0.275f,
    .fKeyRepeatRate           = 0.050f,
    .tMainFramebufferScale   = {1.0f, 1.0f},
    .tCurrentCursor           = PL_MOUSE_CURSOR_ARROW,
    .tNextCursor              = PL_MOUSE_CURSOR_ARROW,
    .tMainViewportSize       = {500.0f, 500.0f},
    .bViewportSizeChanged     = true,
    .bRunning                 = true,
};

// memory tracking
size_t             gszActiveAllocations = 0;
size_t             gszAllocationFrees   = 0;
size_t             gszMemoryUsage       = 0;
plAllocationEntry* gsbtAllocations      = NULL;
plHashMap*         gptMemoryHashMap     = NULL;

//-----------------------------------------------------------------------------
// [SECTION] api registry implementation
//-----------------------------------------------------------------------------

bool
pl__check_apis(void)
{
    bool bResult = true;
    plApiEntry* ptCurrentEntry = gptApiHead;
    while(ptCurrentEntry)
    {

        if(!ptCurrentEntry->bSet) // not set
        {
            printf("[ERROR] Compatible API not found for %u.%u.%u %s\n",
                ptCurrentEntry->tVersion.uMajor, ptCurrentEntry->tVersion.uMinor, ptCurrentEntry->tVersion.uPatch, ptCurrentEntry->pcName);
            bResult = false;
        }
        ptCurrentEntry = ptCurrentEntry->ptNext;
    }

    gbApisDirty = false;
    return bResult;
}

void
pl__set_api(const char* pcName, plVersion tVersion, const void* pInterface, size_t szInterfaceSize)
{
    // see if entry already exists
    plApiEntry* ptCurrentEntry = gptApiHead;
    while(ptCurrentEntry)
    {
        if(strcmp(pcName, ptCurrentEntry->pcName) == 0 && ptCurrentEntry->tVersion.uMajor == tVersion.uMajor)
        {
            // set exact match obviously
            if(ptCurrentEntry->tVersion.uMinor == tVersion.uMinor && ptCurrentEntry->tVersion.uPatch == tVersion.uPatch)
            {
                memcpy(ptCurrentEntry->apBuffer, pInterface, szInterfaceSize);
                ptCurrentEntry->bSet = true;
                
            }
            else if(!ptCurrentEntry->bSet && tVersion.uMajor > 0) // not set, lets see if we are compatible
            {
                // prefer higher minors
                if(ptCurrentEntry->tVersion.uMinor <= tVersion.uMinor)
                {
                    memcpy(ptCurrentEntry->apBuffer, pInterface, szInterfaceSize);
                    ptCurrentEntry->bSet = true;
                    printf("[INFO] API %u.%u %s set with compatible version %u.%u\n",
                        ptCurrentEntry->tVersion.uMajor, ptCurrentEntry->tVersion.uMinor, pcName, tVersion.uMajor, tVersion.uMinor);
                }
            }
        }

        ptCurrentEntry = ptCurrentEntry->ptNext;
    }

    ptCurrentEntry = PL_ALLOC(sizeof(plApiEntry));
    memset(ptCurrentEntry, 0, sizeof(plApiEntry)); 

    ptCurrentEntry->pcName = pcName;
    ptCurrentEntry->tVersion = tVersion;
    ptCurrentEntry->bSet = true;
    memcpy(ptCurrentEntry->apBuffer, pInterface, szInterfaceSize);
    ptCurrentEntry->ptNext = gptApiHead;
    gptApiHead = ptCurrentEntry;
}

const void*
pl__get_api(const char* pcName, plVersion tVersion)
{

    // first look for perfect match
    plApiEntry* ptCurrentEntry = gptApiHead;
    while(ptCurrentEntry)
    {
        if(strcmp(pcName, ptCurrentEntry->pcName) == 0 &&
            ptCurrentEntry->tVersion.uMajor == tVersion.uMajor &&
            ptCurrentEntry->tVersion.uMinor == tVersion.uMinor &&
            ptCurrentEntry->tVersion.uPatch == tVersion.uPatch)
        {
            return ptCurrentEntry->apBuffer;
        }

        ptCurrentEntry = ptCurrentEntry->ptNext;
    }

    // if API is stable, we can try again by looking for compatible versions
    if(tVersion.uMajor > 0)
    {
        ptCurrentEntry = gptApiHead;
        while(ptCurrentEntry)
        {
            if(strcmp(pcName, ptCurrentEntry->pcName) == 0)
            {   
                if(ptCurrentEntry->tVersion.uMajor == tVersion.uMajor && ptCurrentEntry->tVersion.uMinor >= tVersion.uMinor)
                {
                    printf("[INFO] API %u.%u %s requested but received compatible version %u.%u\n",
                        tVersion.uMajor, tVersion.uMinor, pcName, ptCurrentEntry->tVersion.uMajor, ptCurrentEntry->tVersion.uMinor);
                    return ptCurrentEntry->apBuffer;
                }
            } 
            ptCurrentEntry = ptCurrentEntry->ptNext;
        }
    }

    // none found, create entry
    ptCurrentEntry = PL_ALLOC(sizeof(plApiEntry));
    memset(ptCurrentEntry, 0, sizeof(plApiEntry)); 
    ptCurrentEntry->pcName = pcName;
    ptCurrentEntry->tVersion = tVersion;
    ptCurrentEntry->ptNext = gptApiHead;
    gptApiHead = ptCurrentEntry;
    gbApisDirty = true;
    return ptCurrentEntry->apBuffer;
}

void
pl__remove_api(const void* pInterface)
{
    plApiEntry* ptLastEntry = NULL;
    plApiEntry* ptCurrentEntry = gptApiHead;
    while(ptCurrentEntry)
    {
        if(ptCurrentEntry->apBuffer == pInterface)
        {
            if(ptLastEntry)
                ptLastEntry->ptNext = ptCurrentEntry->ptNext;
            else
                gptApiHead = ptCurrentEntry->ptNext;

            PL_FREE(ptCurrentEntry);
            return;
        }

        ptLastEntry = ptCurrentEntry;
        ptCurrentEntry = ptCurrentEntry->ptNext;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] data registry implementation
//-----------------------------------------------------------------------------

void
pl_set_data(const char* pcName, void* pData)
{
    plDataID tData = {
        .ulData = pl_hm_lookup_str(gptHashmap, pcName)
    };

    if(tData.ulData == UINT64_MAX)
    {
        tData = pl_create_object();
    }
    plDataObject* ptWriter = pl_write(tData);
    pl_set_string(ptWriter, 0, pcName);
    pl_set_buffer(ptWriter, 1, pData);
    pl_commit(ptWriter);
}

void*
pl_get_data(const char* pcName)
{
    plDataID tData = pl_get_object_by_name(pcName);
    if(tData.ulData == UINT64_MAX)
        return NULL;
    const plDataObject* ptReader = pl_read(tData);
    void* pData = pl_get_buffer(ptReader, 1);
    pl_end_read(ptReader);
    return pData;
}

void
pl__garbage_collect_data_reg(void)
{
    pl_lock_mutex(gptDataMutex);
    uint32_t uQueueSize = pl_sb_size(gtDataRegistryData.sbtDataObjectsDeletionQueue);
    for(uint32_t i = 0; i < uQueueSize; i++)
    {
        if(gtDataRegistryData.sbtDataObjectsDeletionQueue[i]->uReferenceCount == 0)
        {
            pl_sb_push(gtDataRegistryData.sbtDataObjects, gtDataRegistryData.sbtDataObjectsDeletionQueue[i]);
            pl_sb_del_swap(gtDataRegistryData.sbtDataObjectsDeletionQueue, i);
            i--;
            uQueueSize--;
        }
    }
    pl_unlock_mutex(gptDataMutex);
}

plDataID
pl_create_object(void)
{
    plDataID tId = {.ulData = UINT64_MAX};

    pl_lock_mutex(gptDataMutex);
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
    pl_unlock_mutex(gptDataMutex);
    ptObject->tId = tId;

    ptObject->uPropertyCount = 2;
    ptObject->ptProperties = ptObject->atDefaultProperties;
    ptObject->atDefaultProperties[0].pcValue = NULL;
    ptObject->atDefaultProperties[1].pValue = NULL;

    gtDataRegistryData.aptObjects[tId.uIndex] = ptObject;

    return tId;
}

plDataID
pl_get_object_by_name(const char* pcName)
{
    plDataID tID = {
        .ulData = pl_hm_lookup_str(gptHashmap, pcName)
    };
    return tID;
}

const plDataObject*
pl_read(plDataID tId)
{
    gtDataRegistryData.aptObjects[tId.uIndex]->uReferenceCount++;
    return gtDataRegistryData.aptObjects[tId.uIndex];
}

void
pl_end_read(const plDataObject* ptReader)
{
    gtDataRegistryData.aptObjects[ptReader->tId.uIndex]->uReferenceCount--;
}

const char*
pl_get_string(const plDataObject* ptReader, uint32_t uProperty)
{
    return ptReader->ptProperties[uProperty].pcValue;
}

void*
pl_get_buffer(const plDataObject* ptReader, uint32_t uProperty)
{
    return ptReader->ptProperties[uProperty].pValue;
}

plDataObject*
pl_write(plDataID tId)
{
    const plDataObject* ptOriginalObject = gtDataRegistryData.aptObjects[tId.uIndex];

    pl_lock_mutex(gptDataMutex);
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
    pl_unlock_mutex(gptDataMutex);

    memcpy(ptObject, ptOriginalObject, sizeof(plDataObject));
    ptObject->uReferenceCount = 0;
    ptObject->ptProperties = ptObject->atDefaultProperties;

    return ptObject;
}

void
pl_set_string(plDataObject* ptWriter, uint32_t uProperty, const char* pcValue)
{
    ptWriter->ptProperties[uProperty].pcValue = pcValue;
    if(uProperty == 0)
    {
        if(pl_hm_has_key_str(gptHashmap, pcValue))
        {
            pl_hm_remove_str(gptHashmap, pcValue);
        }
        else
        {
            pl_hm_insert_str(gptHashmap, pcValue, ptWriter->tId.ulData);
        }
    }
}

void
pl_set_buffer(plDataObject* ptWriter, uint32_t uProperty, void* pData)
{
    ptWriter->ptProperties[uProperty].pValue = pData;
}

void
pl_commit(plDataObject* ptWriter)
{
    plDataObject* ptOriginalObject = gtDataRegistryData.aptObjects[ptWriter->tId.uIndex];
    pl_lock_mutex(gptDataMutex);
    pl_sb_push(gtDataRegistryData.sbtDataObjectsDeletionQueue, ptOriginalObject);
    pl_unlock_mutex(gptDataMutex);
    gtDataRegistryData.aptObjects[ptWriter->tId.uIndex] = ptWriter;
}

//-----------------------------------------------------------------------------
// [SECTION] extension registry implementation
//-----------------------------------------------------------------------------

static void
pl__create_extension(const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, plExtension* ptExtensionOut)
{
    strncpy(ptExtensionOut->pcLibPath, pcName, 128);
    strncpy(ptExtensionOut->pcLibName, pcName, 128);
    strncpy(ptExtensionOut->pcLoadFunc, pcLoadFunc, 128);
    strncpy(ptExtensionOut->pcUnloadFunc, pcUnloadFunc, 128);
}

bool
pl_load_extension(const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable)
{

    // check if extension is already loaded
    const uint32_t uCurrentExtensionCount = pl_sb_size(gsbtExtensions);
    for(uint32_t i = 0; i < uCurrentExtensionCount; i++)
    {
        if(strcmp(pcName, gsbtExtensions[i].pcLibName) == 0)
        {
            return true;
        }
    }

    if(pcLoadFunc == NULL)
        pcLoadFunc = "pl_load_ext";

    if(pcUnloadFunc == NULL)
        pcUnloadFunc = "pl_unload_ext";

    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    plExtension tExtension = {
        .bReloadable = bReloadable
    };
    pl__create_extension(pcName, pcLoadFunc, pcUnloadFunc, &tExtension);

    plSharedLibrary* ptLibrary = NULL;

    const plLibraryI* ptLibraryApi = pl_get_api_latest(ptApiRegistry, plLibraryI);

    plLibraryDesc tDesc = {
        .pcName = tExtension.pcLibPath
    };

    if(ptLibraryApi->load(tDesc, &ptLibrary))
    {
        tExtension.pl_load   = (void PL_CALL_CONVENTION (const plApiRegistryI*, bool)) ptLibraryApi->load_function(ptLibrary, tExtension.pcLoadFunc);
        tExtension.pl_unload = (void PL_CALL_CONVENTION (const plApiRegistryI*, bool)) ptLibraryApi->load_function(ptLibrary, tExtension.pcUnloadFunc);
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
        // printf("Extension: %s not loaded\n", tExtension.pcLibPath);
        return false;
    }
    return true;
}

bool
pl_unload_extension(const char* pcName)
{
    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    const uint32_t uExtCount = pl_sb_size(gsbtExtensions);
    const uint32_t uHotLibCount = pl_sb_size(gsbtHotLibs);
    for(uint32_t i = 0; i < uExtCount; i++)
    {
        if(strcmp(pcName, gsbtExtensions[i].pcLibName) == 0)
        {
            gsbtExtensions[i].pl_unload(ptApiRegistry, false);
            PL_FREE(gsbptLibs[i]);
            gsbptLibs[i] = NULL;
            pl_sb_del_swap(gsbtExtensions, i);
            pl_sb_del_swap(gsbptLibs, i);

            // remove from hot libs if reloadable
            if(gsbtExtensions[i].bReloadable)
            {
                for(uint32_t j = 0; j < uHotLibCount; j++)
                {
                    if(gsbtHotLibs[j] == i)
                    {
                        pl_sb_del_swap(gsbtHotLibs, j);
                        break;
                    }
                }
            }
            
            return true;
        }
    }

    return false;
}

void
pl__unload_all_extensions(void)
{
    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    const uint32_t uExtCount = pl_sb_size(gsbtExtensions);
    for(uint32_t i = 0; i < uExtCount; i++)
    {
        if(gsbtExtensions[i].pl_unload)
            gsbtExtensions[i].pl_unload(ptApiRegistry, false);
    }
}

void
pl__handle_extension_reloads(void)
{
    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();

    const uint32_t uHotExtCount = pl_sb_size(gsbtHotLibs);
    for(uint32_t i = 0; i < uHotExtCount; i++)
    {
        if(pl_has_library_changed(gsbptLibs[gsbtHotLibs[i]]))
        {
            plSharedLibrary* ptLibrary = gsbptLibs[gsbtHotLibs[i]];
            plExtension* ptExtension = &gsbtExtensions[gsbtHotLibs[i]];
            ptExtension->pl_unload(ptApiRegistry, true);
            pl_reload_library(ptLibrary); 
            ptExtension->pl_load   = (void PL_CALL_CONVENTION (const plApiRegistryI*, bool)) pl_load_library_function(ptLibrary, ptExtension->pcLoadFunc);
            ptExtension->pl_unload = (void PL_CALL_CONVENTION (const plApiRegistryI*, bool)) pl_load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
            PL_ASSERT(ptExtension->pl_load);
            PL_ASSERT(ptExtension->pl_unload);
            ptExtension->pl_load(ptApiRegistry, true);
        }
            
    }
}

//-----------------------------------------------------------------------------
// [SECTION] io implementation
//-----------------------------------------------------------------------------

static inline plKeyData*
pl__get_key_data(plKey tKey)
{
    if(tKey & PL_KEY_MOD_MASK_)
    {
        if     (tKey == PL_KEY_MOD_CTRL)  tKey = PL_KEY_RESERVED_MOD_CTRL;
        else if(tKey == PL_KEY_MOD_SHIFT) tKey = PL_KEY_RESERVED_MOD_SHIFT;
        else if(tKey == PL_KEY_MOD_ALT)   tKey = PL_KEY_RESERVED_MOD_ALT;
        else if(tKey == PL_KEY_MOD_SUPER) tKey = PL_RESERVED_KEY_MOD_SUPER;
        else if(tKey == PL_KEY_MOD_SHORTCUT) tKey = (gtIO.bConfigMacOSXBehaviors ? PL_RESERVED_KEY_MOD_SUPER : PL_KEY_RESERVED_MOD_CTRL);
    }
    PL_ASSERT(tKey >= PL_KEY_NAMED_KEY_BEGIN && tKey < PL_KEY_NAMED_KEY_END && "Key not valid");
    return &gtIO._tKeyData[tKey - PL_KEY_NAMED_KEY_BEGIN];
}

plVersion
pl_get_version(void)
{
    return PILOT_LIGHT_VERSION;
}

const char*
pl_get_version_string(void)
{
    return PILOT_LIGHT_VERSION_STRING;
}

void
pl_add_key_event(plKey tKey, bool bDown)
{
    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_KEY, (int)tKey);
    if(ptLastEvent && ptLastEvent->bKeyDown == bDown)
        return;

    const plInputEvent tEvent = {
        .tType    = PL_INPUT_EVENT_TYPE_KEY,
        .tSource  = PL_INPUT_EVENT_SOURCE_KEYBOARD,
        .tKey     = tKey,
        .bKeyDown = bDown
    };
    pl_sb_push(gtIO._sbtInputEvents, tEvent);
}

void
pl_add_text_event(uint32_t uChar)
{
    const plInputEvent tEvent = {
        .tType    = PL_INPUT_EVENT_TYPE_TEXT,
        .tSource  = PL_INPUT_EVENT_SOURCE_KEYBOARD,
        .uChar     = uChar
    };
    pl_sb_push(gtIO._sbtInputEvents, tEvent);
}

void
pl_add_text_event_utf16(uint16_t uChar)
{
    if (uChar == 0 && gtIO._tInputQueueSurrogate == 0)
        return;

    if ((uChar & 0xFC00) == 0xD800) // High surrogate, must save
    {
        if (gtIO._tInputQueueSurrogate != 0)
            pl_add_text_event(0xFFFD);
        gtIO._tInputQueueSurrogate = uChar;
        return;
    }

    plUiWChar cp = uChar;
    if (gtIO._tInputQueueSurrogate != 0)
    {
        if ((uChar & 0xFC00) != 0xDC00) // Invalid low surrogate
        {
            pl_add_text_event(0xFFFD);
        }
        else
        {
            cp = 0xFFFD; // Codepoint will not fit in ImWchar
        }

        gtIO._tInputQueueSurrogate = 0;
    }
    pl_add_text_event((uint32_t)cp);
}

void
pl_add_text_events_utf8(const char* pcText)
{
    while(*pcText != 0)
    {
        uint32_t uChar = 0;
        pcText += pl_text_char_from_utf8(&uChar, pcText, NULL);
        pl_add_text_event(uChar);
    }
}

void
pl_add_mouse_pos_event(float fX, float fY)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_POS, (int)(fX + fY));
    if(ptLastEvent && ptLastEvent->fPosX == fX && ptLastEvent->fPosY == fY)
        return;

    const plInputEvent tEvent = {
        .tType    = PL_INPUT_EVENT_TYPE_MOUSE_POS,
        .tSource  = PL_INPUT_EVENT_SOURCE_MOUSE,
        .fPosX    = fX,
        .fPosY    = fY
    };
    pl_sb_push(gtIO._sbtInputEvents, tEvent);
}

void
pl_add_mouse_button_event(int iButton, bool bDown)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_BUTTON, iButton);
    if(ptLastEvent && ptLastEvent->bMouseDown == bDown)
        return;

    const plInputEvent tEvent = {
        .tType      = PL_INPUT_EVENT_TYPE_MOUSE_BUTTON,
        .tSource    = PL_INPUT_EVENT_SOURCE_MOUSE,
        .iButton    = iButton,
        .bMouseDown = bDown
    };
    pl_sb_push(gtIO._sbtInputEvents, tEvent);
}

void
pl_add_mouse_wheel_event(float fX, float fY)
{

    const plInputEvent tEvent = {
        .tType   = PL_INPUT_EVENT_TYPE_MOUSE_WHEEL,
        .tSource = PL_INPUT_EVENT_SOURCE_MOUSE,
        .fWheelX = fX,
        .fWheelY = fY
    };
    pl_sb_push(gtIO._sbtInputEvents, tEvent);
}

void
pl_clear_input_characters(void)
{
    pl_sb_reset(gtIO._sbInputQueueCharacters);
}

bool
pl_is_key_down(plKey tKey)
{
    const plKeyData* ptData = pl__get_key_data(tKey);
    return ptData->bDown;
}

int
pl_get_key_pressed_amount(plKey tKey, float fRepeatDelay, float fRate)
{
    const plKeyData* ptData = pl__get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return 0;
    const float fT = ptData->fDownDuration;
    return pl__calc_typematic_repeat_amount(fT - gtIO.fDeltaTime, fT, fRepeatDelay, fRate);
}

bool
pl_is_key_pressed(plKey tKey, bool bRepeat)
{
    const plKeyData* ptData = pl__get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return false;
    const float fT = ptData->fDownDuration;
    if (fT < 0.0f)
        return false;

    bool bPressed = (fT == 0.0f);
    if (!bPressed && bRepeat)
    {
        const float fRepeatDelay = gtIO.fKeyRepeatDelay;
        const float fRepeatRate = gtIO.fKeyRepeatRate;
        bPressed = (fT > fRepeatDelay) && pl_get_key_pressed_amount(tKey, fRepeatDelay, fRepeatRate) > 0;
    }

    if (!bPressed)
        return false;
    return true;
}

bool
pl_is_key_released(plKey tKey)
{
    const plKeyData* ptData = pl__get_key_data(tKey);
    if (ptData->fDownDurationPrev < 0.0f || ptData->bDown)
        return false;
    return true;
}

bool
pl_is_mouse_down(plMouseButton tButton)
{
    return gtIO._abMouseDown[tButton];
}

bool
pl_is_mouse_clicked(plMouseButton tButton, bool bRepeat)
{
    if(!gtIO._abMouseDown[tButton])
        return false;
    const float fT = gtIO._afMouseDownDuration[tButton];
    if(fT == 0.0f)
        return true;
    if(bRepeat && fT > gtIO.fKeyRepeatDelay)
        return pl__calc_typematic_repeat_amount(fT - gtIO.fDeltaTime, fT, gtIO.fKeyRepeatDelay, gtIO.fKeyRepeatRate) > 0;
    return false;
}

bool
pl_is_mouse_released(plMouseButton tButton)
{
    return gtIO._abMouseReleased[tButton];
}

bool
pl_is_mouse_double_clicked(plMouseButton tButton)
{
    return gtIO._auMouseClickedCount[tButton] == 2;
}

bool
pl_is_mouse_dragging(plMouseButton tButton, float fThreshold)
{
    if(!gtIO._abMouseDown[tButton])
        return false;
    if(fThreshold < 0.0f)
        fThreshold = gtIO.fMouseDragThreshold;
    return gtIO._afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold;
}

bool
pl_is_mouse_hovering_rect(plVec2 minVec, plVec2 maxVec)
{
    const plVec2 tMousePos = gtIO._tMousePos;
    return ( tMousePos.x >= minVec.x && tMousePos.y >= minVec.y && tMousePos.x <= maxVec.x && tMousePos.y <= maxVec.y);
}

void
pl_reset_mouse_drag_delta(plMouseButton tButton)
{
    gtIO._atMouseClickedPos[tButton] = gtIO._tMousePos;
}

plVec2
pl_get_mouse_pos(void)
{
    return gtIO._tMousePos;
}

float
pl_get_mouse_wheel(void)
{
    return gtIO._fMouseWheel;
}

bool
pl_is_mouse_pos_valid(plVec2 tPos)
{
    return tPos.x > -FLT_MAX && tPos.y > -FLT_MAX;
}

void
pl_set_mouse_cursor(plMouseCursor tCursor)
{
    gtIO.tNextCursor = tCursor;
    gtIO.bCursorChanged = true;
}

plVec2
pl_get_mouse_drag_delta(plMouseButton tButton, float fThreshold)
{
    if(fThreshold < 0.0f)
        fThreshold = gtIO.fMouseDragThreshold;
    if(gtIO._abMouseDown[tButton] || gtIO._abMouseReleased[tButton])
    {
        if(gtIO._afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold)
        {
            if(pl_is_mouse_pos_valid(gtIO._tMousePos) && pl_is_mouse_pos_valid(gtIO._atMouseClickedPos[tButton]))
                return pl_sub_vec2(gtIO._tLastValidMousePos, gtIO._atMouseClickedPos[tButton]);
        }
    }
    
    return pl_create_vec2(0.0f, 0.0f);
}

plIO*
pl_get_io(void)
{
    return &gtIO;
}

static void
pl__update_events(void)
{
    const uint32_t uEventCount = pl_sb_size(gtIO._sbtInputEvents);
    for(uint32_t i = 0; i < uEventCount; i++)
    {
        plInputEvent* ptEvent = &gtIO._sbtInputEvents[i];

        switch(ptEvent->tType)
        {
            case PL_INPUT_EVENT_TYPE_MOUSE_POS:
            {
                // PL_UI_DEBUG_LOG_IO("[%Iu] IO Mouse Pos (%0.0f, %0.0f)", gptCtx->frameCount, ptEvent->fPosX, ptEvent->fPosY);

                if(ptEvent->fPosX != -FLT_MAX && ptEvent->fPosY != -FLT_MAX)
                {
                    gtIO._tMousePos.x = ptEvent->fPosX;
                    gtIO._tMousePos.y = ptEvent->fPosY;
                }
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_WHEEL:
            {
                // PL_UI_DEBUG_LOG_IO("[%Iu] IO Mouse Wheel (%0.0f, %0.0f)", gptCtx->frameCount, ptEvent->fWheelX, ptEvent->fWheelY);
                gtIO._fMouseWheelH += ptEvent->fWheelX;
                gtIO._fMouseWheel += ptEvent->fWheelY;
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_BUTTON:
            {
                // PL_UI_DEBUG_LOG_IO(ptEvent->bMouseDown ? "[%Iu] IO Mouse Button %i down" : "[%Iu] IO Mouse Button %i up", gptCtx->frameCount, ptEvent->iButton);
                PL_ASSERT(ptEvent->iButton >= 0 && ptEvent->iButton < PL_MOUSE_BUTTON_COUNT);
                gtIO._abMouseDown[ptEvent->iButton] = ptEvent->bMouseDown;
                break;
            }

            case PL_INPUT_EVENT_TYPE_KEY:
            {
                // if(ptEvent->tKey < PL_KEY_COUNT)
                //     PL_UI_DEBUG_LOG_IO(ptEvent->bKeyDown ? "[%Iu] IO Key %i down" : "[%Iu] IO Key %i up", gptCtx->frameCount, ptEvent->tKey);
                plKey tKey = ptEvent->tKey;
                if(tKey != PL_KEY_NONE)
                {
                    plKeyData* ptKeyData = pl__get_key_data(tKey);
                    ptKeyData->bDown = ptEvent->bKeyDown;
                }
                break;
            }

            case PL_INPUT_EVENT_TYPE_TEXT:
            {
                // PL_UI_DEBUG_LOG_IO("[%Iu] IO Text (U+%08u)", gptCtx->frameCount, (uint32_t)ptEvent->uChar);
                plUiWChar uChar = (plUiWChar)ptEvent->uChar;
                pl_sb_push(gtIO._sbInputQueueCharacters, uChar);
                break;
            }

            default:
            {
                PL_ASSERT(false && "unknown input event type");
                break;
            }
        }
    }
    pl_sb_reset(gtIO._sbtInputEvents)
}

static void
pl__update_keyboard_inputs(void)
{
    gtIO.tKeyMods = 0;
    if (pl_is_key_down(PL_KEY_LEFT_CTRL)  || pl_is_key_down(PL_KEY_RIGHT_CTRL))     { gtIO.tKeyMods |= PL_KEY_MOD_CTRL; }
    if (pl_is_key_down(PL_KEY_LEFT_SHIFT) || pl_is_key_down(PL_KEY_RIGHT_SHIFT))    { gtIO.tKeyMods |= PL_KEY_MOD_SHIFT; }
    if (pl_is_key_down(PL_KEY_LEFT_ALT)   || pl_is_key_down(PL_KEY_RIGHT_ALT))      { gtIO.tKeyMods |= PL_KEY_MOD_ALT; }
    if (pl_is_key_down(PL_KEY_LEFT_SUPER) || pl_is_key_down(PL_KEY_RIGHT_SUPER))    { gtIO.tKeyMods |= PL_KEY_MOD_SUPER; }

    gtIO.bKeyCtrl  = (gtIO.tKeyMods & PL_KEY_MOD_CTRL) != 0;
    gtIO.bKeyShift = (gtIO.tKeyMods & PL_KEY_MOD_SHIFT) != 0;
    gtIO.bKeyAlt   = (gtIO.tKeyMods & PL_KEY_MOD_ALT) != 0;
    gtIO.bKeySuper = (gtIO.tKeyMods & PL_KEY_MOD_SUPER) != 0;

    // Update keys
    for (uint32_t i = 0; i < PL_KEY_COUNT; i++)
    {
        plKeyData* ptKeyData = &gtIO._tKeyData[i];
        ptKeyData->fDownDurationPrev = ptKeyData->fDownDuration;
        ptKeyData->fDownDuration = ptKeyData->bDown ? (ptKeyData->fDownDuration < 0.0f ? 0.0f : ptKeyData->fDownDuration + gtIO.fDeltaTime) : -1.0f;
    }
}

static void
pl__update_mouse_inputs(void)
{
    if(pl_is_mouse_pos_valid(gtIO._tMousePos))
    {
        gtIO._tMousePos.x = floorf(gtIO._tMousePos.x);
        gtIO._tMousePos.y = floorf(gtIO._tMousePos.y);
        gtIO._tLastValidMousePos = gtIO._tMousePos;
    }

    // only calculate data if the current & previous mouse position are valid
    if(pl_is_mouse_pos_valid(gtIO._tMousePos) && pl_is_mouse_pos_valid(gtIO._tMousePosPrev))
        gtIO._tMouseDelta = pl_sub_vec2(gtIO._tMousePos, gtIO._tMousePosPrev);
    else
    {
        gtIO._tMouseDelta.x = 0.0f;
        gtIO._tMouseDelta.y = 0.0f;
    }
    gtIO._tMousePosPrev = gtIO._tMousePos;

    for(uint32_t i = 0; i < PL_MOUSE_BUTTON_COUNT; i++)
    {
        gtIO._abMouseClicked[i] = gtIO._abMouseDown[i] && gtIO._afMouseDownDuration[i] < 0.0f;
        gtIO._auMouseClickedCount[i] = 0;
        gtIO._abMouseReleased[i] = !gtIO._abMouseDown[i] && gtIO._afMouseDownDuration[i] >= 0.0f;
        gtIO._afMouseDownDurationPrev[i] = gtIO._afMouseDownDuration[i];
        gtIO._afMouseDownDuration[i] = gtIO._abMouseDown[i] ? (gtIO._afMouseDownDuration[i] < 0.0f ? 0.0f : gtIO._afMouseDownDuration[i] + gtIO.fDeltaTime) : -1.0f;

        if(gtIO._abMouseClicked[i])
        {

            bool bIsRepeatedClick = false;
            if((float)(gtIO.dTime - gtIO._adMouseClickedTime[i]) < gtIO.fMouseDoubleClickTime)
            {
                plVec2 tDeltaFromClickPos = pl_create_vec2(0.0f, 0.0f);
                if(pl_is_mouse_pos_valid(gtIO._tMousePos))
                    tDeltaFromClickPos = pl_sub_vec2(gtIO._tMousePos, gtIO._atMouseClickedPos[i]);

                if(PL_VEC2_LENGTH_SQR(tDeltaFromClickPos) < gtIO.fMouseDoubleClickMaxDist * gtIO.fMouseDoubleClickMaxDist)
                    bIsRepeatedClick = true;
            }

            if(bIsRepeatedClick)
                gtIO._auMouseClickedLastCount[i]++;
            else
                gtIO._auMouseClickedLastCount[i] = 1;

            gtIO._adMouseClickedTime[i] = gtIO.dTime;
            gtIO._atMouseClickedPos[i] = gtIO._tMousePos;
            gtIO._afMouseDragMaxDistSqr[i] = 0.0f;
            gtIO._auMouseClickedCount[i] = gtIO._auMouseClickedLastCount[i];
        }
        else if(gtIO._abMouseDown[i])
        {
            const plVec2 tClickPos = pl_sub_vec2(gtIO._tLastValidMousePos, gtIO._atMouseClickedPos[i]);
            float fDeltaSqrClickPos = PL_VEC2_LENGTH_SQR(tClickPos);
            gtIO._afMouseDragMaxDistSqr[i] = pl_max(fDeltaSqrClickPos, gtIO._afMouseDragMaxDistSqr[i]);
        }
    }
}

static int
pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate)
{
    if(fT1 == 0.0f)
        return 1;
    if(fT0 >= fT1)
        return 0;
    if(fRepeatRate <= 0.0f)
        return (fT0 < fRepeatDelay) && (fT1 >= fRepeatDelay);
    
    const int iCountT0 = (fT0 < fRepeatDelay) ? -1 : (int)((fT0 - fRepeatDelay) / fRepeatRate);
    const int iCountT1 = (fT1 < fRepeatDelay) ? -1 : (int)((fT1 - fRepeatDelay) / fRepeatRate);
    const int iCount = iCountT1 - iCountT0;
    return iCount;
}

static plInputEvent*
pl__get_last_event(plInputEventType tType, int iButtonOrKey)
{
    const uint32_t uEventCount = pl_sb_size(gtIO._sbtInputEvents);
    for(uint32_t i = 0; i < uEventCount; i++)
    {
        plInputEvent* ptEvent = &gtIO._sbtInputEvents[uEventCount - i - 1];
        if(ptEvent->tType != tType)
            continue;
        if(tType == PL_INPUT_EVENT_TYPE_KEY && (int)ptEvent->tKey != iButtonOrKey)
            continue;
        else if(tType == PL_INPUT_EVENT_TYPE_MOUSE_BUTTON && ptEvent->iButton != iButtonOrKey)
            continue;
        else if(tType == PL_INPUT_EVENT_TYPE_MOUSE_POS && (int)(ptEvent->fPosX + ptEvent->fPosY) != iButtonOrKey)
            continue;
        return ptEvent;
    }
    return NULL;
}

void
pl_new_frame(void)
{

    // update IO structure
    gtIO.dTime += (double)gtIO.fDeltaTime;
    gtIO.ulFrameCount++;
    gtIO.bViewportSizeChanged = false;

    // calculate frame rate
    gtIO._fFrameRateSecPerFrameAccum += gtIO.fDeltaTime - gtIO._afFrameRateSecPerFrame[gtIO._iFrameRateSecPerFrameIdx];
    gtIO._afFrameRateSecPerFrame[gtIO._iFrameRateSecPerFrameIdx] = gtIO.fDeltaTime;
    gtIO._iFrameRateSecPerFrameIdx = (gtIO._iFrameRateSecPerFrameIdx + 1) % 120;
    gtIO._iFrameRateSecPerFrameCount = pl_max(gtIO._iFrameRateSecPerFrameCount, 120);
    gtIO.fFrameRate = FLT_MAX;
    if(gtIO._fFrameRateSecPerFrameAccum > 0)
        gtIO.fFrameRate = ((float) gtIO._iFrameRateSecPerFrameCount) / gtIO._fFrameRateSecPerFrameAccum;

    // handle events
    pl__update_events();
    pl__update_keyboard_inputs();
    pl__update_mouse_inputs();
}

//-----------------------------------------------------------------------------
// [SECTION] memory api implementation
//-----------------------------------------------------------------------------

size_t
pl_get_memory_usage(void)
{
    return gszMemoryUsage;
}

size_t
pl_get_allocation_count(void)
{
    return gszActiveAllocations;
}

size_t
pl_get_free_count(void)
{
    return gszAllocationFrees;
}

plAllocationEntry*
pl_get_allocations(size_t* pszCount)
{
    *pszCount = pl_sb_size(gsbtAllocations);
    return gsbtAllocations;
}

void
pl__check_for_leaks(void)
{
    #ifdef PL_MEMORY_TRACKING_ON
    // check for unfreed memory
    uint32_t uMemoryLeakCount = 0;
    for(uint32_t i = 0; i < pl_sb_size(gsbtAllocations); i++)
    {
        if(gsbtAllocations[i].pAddress != NULL)
        {
            printf("Unfreed memory from line %i in file '%s'.\n", gsbtAllocations[i].iLine, gsbtAllocations[i].pcFile);
            uMemoryLeakCount++;
        }
    }
        
    PL_ASSERT(uMemoryLeakCount == gszActiveAllocations);
    if(uMemoryLeakCount > 0)
        printf("%u unfreed allocations.\n", uMemoryLeakCount);
    #else
        if(gszActiveAllocations > 0)
            printf("%u unfreed allocations.\n", (uint32_t)gszActiveAllocations);
        PL_ASSERT(gszActiveAllocations == 0);
    #endif
}

void*
pl_realloc(void* pBuffer, size_t szSize)
{

    pl_lock_mutex(gptMemoryMutex);

    void* pNewBuffer = NULL;

    if(szSize > 0)
        gszActiveAllocations++;

    if(pBuffer)
    {
        gszAllocationFrees++;
        gszActiveAllocations--;
    }

    if(szSize == 0 && pBuffer)
        free(pBuffer);
    else if(szSize > 0)
    {
        pNewBuffer = realloc(pBuffer, szSize);
        memset(pNewBuffer, 0, szSize);
    }

    pl_unlock_mutex(gptMemoryMutex);
    return pNewBuffer;
}

void*
pl_tracked_realloc(void* pBuffer, size_t szSize, const char* pcFile, int iLine)
{

    pl_lock_mutex(gptMemoryMutex);

    void* pNewBuffer = NULL;

    #ifdef PL_MEMORY_TRACKING_ON

    size_t szOldSize = 0;

    if(pBuffer) // free
    {

            const uint64_t ulHash = pl_hm_hash(&pBuffer, sizeof(void*), 1);
            const bool bDataExists = pl_hm_has_key(gptMemoryHashMap, ulHash);

            if(bDataExists)
            {
                
                const uint64_t ulIndex = pl_hm_lookup(gptMemoryHashMap, ulHash);
                gsbtAllocations[ulIndex].pAddress = NULL;
                szOldSize = gsbtAllocations[ulIndex].szSize;
                gszMemoryUsage -= gsbtAllocations[ulIndex].szSize;
                gsbtAllocations[ulIndex].szSize = 0;
                pl_hm_remove(gptMemoryHashMap, ulHash);
                gszAllocationFrees++;
                gszActiveAllocations--;
            }
            else
            {
                PL_ASSERT(false);
            }
    }

    if(szSize > 0)
    {
        
        gszActiveAllocations++;
        gszMemoryUsage += szSize;
        pNewBuffer = malloc(szSize);
        memset(pNewBuffer, 0, szSize);

        if(pBuffer)
        {
            memcpy(pNewBuffer, pBuffer, szOldSize);
            free(pBuffer);
        }

        const uint64_t ulHash = pl_hm_hash(&pNewBuffer, sizeof(void*), 1);

        uint64_t ulFreeIndex = pl_hm_get_free_index(gptMemoryHashMap);
        if(ulFreeIndex == UINT64_MAX)
        {
            pl_sb_push(gsbtAllocations, (plAllocationEntry){0});
            ulFreeIndex = pl_sb_size(gsbtAllocations) - 1;
        }
        pl_hm_insert(gptMemoryHashMap, ulHash, ulFreeIndex);
        
        gsbtAllocations[ulFreeIndex].iLine = iLine;
        gsbtAllocations[ulFreeIndex].pcFile = pcFile;
        gsbtAllocations[ulFreeIndex].pAddress = pNewBuffer;
        gsbtAllocations[ulFreeIndex].szSize = szSize;        
    }


    #else

        if(szSize > 0)
            gszActiveAllocations++;

        if(pBuffer)
        {
            gszAllocationFrees++;
            gszActiveAllocations--;
        }

        if(szSize == 0 && pBuffer)
            free(pBuffer);
        else if(szSize > 0)
        {
            pNewBuffer = realloc(pBuffer, szSize);
            memset(pNewBuffer, 0, szSize);
        }
    
    #endif // PL_MEMORY_TRACKING_ON

    pl_unlock_mutex(gptMemoryMutex);
    return pNewBuffer;
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

const plApiRegistryI*
pl__load_api_registry(void)
{
    static const plApiRegistryI tApiRegistry = {
        .get_api    = pl__get_api,
        .set_api    = pl__set_api,
        .remove_api = pl__remove_api
    };

    return &tApiRegistry;
}

void
pl__load_core_apis(void)
{

    const plApiRegistryI* ptApiRegistry = pl__load_api_registry();
    pl_create_mutex(&gptDataMutex);
    pl_create_mutex(&gptMemoryMutex);

    pl_sb_resize(gtDataRegistryData.sbtFreeDataIDs, 1024);
    for(uint32_t i = 0; i < 1024; i++)
    {
        gtDataRegistryData.sbtFreeDataIDs[i].uIndex = i;
    }

    const plIOI tIOApi = {
        .new_frame               = pl_new_frame,
        .get_io                  = pl_get_io,
        .is_key_down             = pl_is_key_down,
        .is_key_pressed          = pl_is_key_pressed,
        .is_key_released         = pl_is_key_released,
        .get_key_pressed_amount  = pl_get_key_pressed_amount,
        .is_mouse_down           = pl_is_mouse_down,
        .is_mouse_clicked        = pl_is_mouse_clicked,
        .is_mouse_released       = pl_is_mouse_released,
        .is_mouse_double_clicked = pl_is_mouse_double_clicked,
        .is_mouse_dragging       = pl_is_mouse_dragging,
        .is_mouse_hovering_rect  = pl_is_mouse_hovering_rect,
        .reset_mouse_drag_delta  = pl_reset_mouse_drag_delta,
        .get_mouse_drag_delta    = pl_get_mouse_drag_delta,
        .get_mouse_pos           = pl_get_mouse_pos,
        .get_mouse_wheel         = pl_get_mouse_wheel,
        .is_mouse_pos_valid      = pl_is_mouse_pos_valid,
        .set_mouse_cursor        = pl_set_mouse_cursor,
        .add_key_event           = pl_add_key_event,
        .add_text_event          = pl_add_text_event,
        .add_text_event_utf16    = pl_add_text_event_utf16,
        .add_text_events_utf8    = pl_add_text_events_utf8,
        .add_mouse_pos_event     = pl_add_mouse_pos_event,
        .add_mouse_button_event  = pl_add_mouse_button_event,
        .add_mouse_wheel_event   = pl_add_mouse_wheel_event,
        .clear_input_characters  = pl_clear_input_characters,
        .get_version             = pl_get_version,
        .get_version_string      = pl_get_version_string
    };

    const plDataRegistryI tDataRegistryApi = {
        .set_data           = pl_set_data,
        .get_data           = pl_get_data,
        .create_object      = pl_create_object,
        .get_object_by_name = pl_get_object_by_name,
        .read               = pl_read,
        .end_read           = pl_end_read,
        .get_string         = pl_get_string,
        .get_buffer         = pl_get_buffer,
        .write              = pl_write,
        .set_string         = pl_set_string,
        .set_buffer         = pl_set_buffer,
        .commit             = pl_commit
    };

    const plExtensionRegistryI tExtensionRegistryApi = {
        .load   = pl_load_extension,
        .unload = pl_unload_extension
    };

    static const plMemoryI tMemoryApi = {
        .realloc              = pl_realloc,
        .tracked_realloc      = pl_tracked_realloc,
        .get_allocation_count = pl_get_allocation_count,
        .get_memory_usage     = pl_get_memory_usage,
        .get_free_count       = pl_get_free_count,
        .get_allocations      = pl_get_allocations
    };

    gptMemory = &tMemoryApi;

    // core apis
    pl_set_api(ptApiRegistry, plIOI, &tIOApi);
    pl_set_api(ptApiRegistry, plDataRegistryI, &tDataRegistryApi);
    pl_set_api(ptApiRegistry, plExtensionRegistryI, &tExtensionRegistryApi);
    pl_set_api(ptApiRegistry, plMemoryI, &tMemoryApi);

    // load apis
    gptDataRegistry      = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptIOI               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptApiRegistry       = ptApiRegistry;
}

void
pl__unload_core_apis(void)
{

    // data registry
    for(uint32_t i = 0; i < pl_sb_size(gtDataRegistryData.sbtDataObjects); i++)
    {
        PL_FREE(gtDataRegistryData.sbtDataObjects[i]);
    }
    for(uint32_t i = 0; i < pl_sb_size(gtDataRegistryData.sbtDataObjectsDeletionQueue); i++)
    {
        PL_FREE(gtDataRegistryData.sbtDataObjectsDeletionQueue[i]);
    }
    for(uint32_t i = 0; i < 1024; i++)
    {
        if(gtDataRegistryData.aptObjects[i])
        {
            PL_FREE(gtDataRegistryData.aptObjects[i]);
            gtDataRegistryData.aptObjects[i] = NULL;
        }
    }

    pl_sb_free(gtDataRegistryData.sbtDataObjects);
    pl_sb_free(gtDataRegistryData.sbtDataObjectsDeletionQueue);
    pl_destroy_mutex(&gptDataMutex);
    pl_hm_free(gptHashmap);

    // api registry
    plApiEntry* ptCurrentEntry = gptApiHead;
    while(ptCurrentEntry)
    {
        plApiEntry* ptOldEntry = ptCurrentEntry;
        ptCurrentEntry = ptCurrentEntry->ptNext;
        PL_FREE(ptOldEntry);
    }

    // extension registry
    pl_sb_free(gsbtExtensions);
    const uint32_t uLibCount = pl_sb_size(gsbptLibs);
    for(uint32_t i = 0; i < uLibCount; i++)
    {
        PL_FREE(gsbptLibs[i]);
    }
    pl_sb_free(gsbptLibs);
    pl_sb_free(gsbtHotLibs);
}

void
pl__load_ext_apis(void)
{

    const plWindowI tWindowApi = {
        .create_window  = pl_create_window,
        .destroy_window = pl_destroy_window
    };

    const plLibraryI tLibraryApi = {
        .has_changed   = pl_has_library_changed,
        .load          = pl_load_library,
        .load_function = pl_load_library_function,
        .reload        = pl_reload_library
    };

    const plFileI tFileApi = {
        .copy         = pl_copy_file,
        .exists       = pl_file_exists,
        .delete       = pl_file_delete,
        .binary_read  = pl_binary_read_file,
        .binary_write = pl_binary_write_file
    };

    const plNetworkI tNetworkApi = {
        .create_address       = pl_create_address,
        .destroy_address      = pl_destroy_address,
        .create_socket        = pl_create_socket,
        .destroy_socket       = pl_destroy_socket,
        .bind_socket          = pl_bind_socket,
        .send_socket_data_to  = pl_send_socket_data_to,
        .get_socket_data_from = pl_get_socket_data_from,
        .connect_socket       = pl_connect_socket,
        .get_socket_data      = pl_get_socket_data,
        .listen_socket        = pl_listen_socket,
        .select_sockets       = pl_select_sockets,
        .accept_socket        = pl_accept_socket,
        .send_socket_data     = pl_send_socket_data,
    };

    const plThreadsI tThreadApi = {
        .get_hardware_thread_count   = pl_get_hardware_thread_count,
        .create_thread               = pl_create_thread,
        .destroy_thread              = pl_destroy_thread,
        .join_thread                 = pl_join_thread,
        .yield_thread                = pl_yield_thread,
        .sleep_thread                = pl_sleep,
        .get_thread_id               = pl_get_thread_id,
        .get_current_thread_id       = pl_get_current_thread_id,
        .create_mutex                = pl_create_mutex,
        .destroy_mutex               = pl_destroy_mutex,
        .lock_mutex                  = pl_lock_mutex,
        .unlock_mutex                = pl_unlock_mutex,
        .create_semaphore            = pl_create_semaphore,
        .destroy_semaphore           = pl_destroy_semaphore,
        .wait_on_semaphore           = pl_wait_on_semaphore,
        .try_wait_on_semaphore       = pl_try_wait_on_semaphore,
        .release_semaphore           = pl_release_semaphore,
        .allocate_thread_local_key   = pl_allocate_thread_local_key,
        .allocate_thread_local_data  = pl_allocate_thread_local_data,
        .free_thread_local_key       = pl_free_thread_local_key, 
        .get_thread_local_data       = pl_get_thread_local_data, 
        .free_thread_local_data      = pl_free_thread_local_data, 
        .create_critical_section     = pl_create_critical_section,
        .destroy_critical_section    = pl_destroy_critical_section,
        .enter_critical_section      = pl_enter_critical_section,
        .leave_critical_section      = pl_leave_critical_section,
        .create_condition_variable   = pl_create_condition_variable,
        .destroy_condition_variable  = pl_destroy_condition_variable,
        .wake_condition_variable     = pl_wake_condition_variable,
        .wake_all_condition_variable = pl_wake_all_condition_variable,
        .sleep_condition_variable    = pl_sleep_condition_variable,
        .create_barrier              = pl_create_barrier,
        .destroy_barrier             = pl_destroy_barrier,
        .wait_on_barrier             = pl_wait_on_barrier
    };

    const plAtomicsI tAtomicsApi = {
        .create_atomic_counter   = pl_create_atomic_counter,
        .destroy_atomic_counter  = pl_destroy_atomic_counter,
        .atomic_store            = pl_atomic_store,
        .atomic_load             = pl_atomic_load,
        .atomic_compare_exchange = pl_atomic_compare_exchange,
        .atomic_increment        = pl_atomic_increment,
        .atomic_decrement        = pl_atomic_decrement
    };

    const plVirtualMemoryI tVirtualMemoryApi = {
        .get_page_size = pl_get_page_size,
        .alloc         = pl_virtual_alloc,
        .reserve       = pl_virtual_reserve,
        .commit        = pl_virtual_commit,
        .uncommit      = pl_virtual_uncommit,
        .free          = pl_virtual_free,
    };

    pl_set_api(gptApiRegistry, plFileI, &tFileApi);
    pl_set_api(gptApiRegistry, plWindowI, &tWindowApi);
    pl_set_api(gptApiRegistry, plLibraryI, &tLibraryApi);
    pl_set_api(gptApiRegistry, plVirtualMemoryI, &tVirtualMemoryApi);
    pl_set_api(gptApiRegistry, plAtomicsI, &tAtomicsApi);
    pl_set_api(gptApiRegistry, plThreadsI, &tThreadApi);
    pl_set_api(gptApiRegistry, plNetworkI, &tNetworkApi);

    gptNetwork       = pl_get_api_latest(gptApiRegistry, plNetworkI);
    gptThreads       = pl_get_api_latest(gptApiRegistry, plThreadsI);
    gptAtomics       = pl_get_api_latest(gptApiRegistry, plAtomicsI);
    gptVirtualMemory = pl_get_api_latest(gptApiRegistry, plVirtualMemoryI);
}

void
pl__unload_ext_apis(void)
{
    gptApiRegistry->remove_api(gptNetwork);
    gptApiRegistry->remove_api(gptThreads);
    gptApiRegistry->remove_api(gptAtomics);
    gptApiRegistry->remove_api(gptVirtualMemory);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

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