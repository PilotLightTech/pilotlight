/*
   pilotlight.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] contexts
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PILOTLIGHT_H
#define PILOTLIGHT_H

#define PILOTLIGHT_VERSION    "1.0.0a"
#define PILOTLIGHT_VERSION_NUM 100000

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryI plApiRegistryI;

#define PL_API_DATA_REGISTRY "PL_API_DATA_REGISTRY"
typedef struct _plDataRegistryI plDataRegistryI;

#define PL_API_EXTENSION_REGISTRY "PL_API_EXTENSION_REGISTRY"
typedef struct _plExtensionRegistryI plExtensionRegistryI;

//-----------------------------------------------------------------------------
// [SECTION] contexts
//-----------------------------------------------------------------------------

#define PL_CONTEXT_MEMORY "PL_CONTEXT_MEMORY"

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef void (*ptApiUpdateCallback)(const void*, const void*, void*);

// types
typedef struct _plMemoryContext   plMemoryContext;
typedef struct _plAllocationEntry plAllocationEntry;
typedef struct _plDataObject      plDataObject;
typedef union  _plDataID          plDataID;

// external forward declarations
typedef struct _plHashMap plHashMap; // pl_ds.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// API registry
const plApiRegistryI* pl_load_core_apis  (void); // only called once by backend
void                  pl_unload_core_apis(void); // only called once by backend

// memory
void             pl_set_memory_context(plMemoryContext* ptMemoryContext);
plMemoryContext* pl_get_memory_context(void);
void*            pl_realloc           (void* pBuffer, size_t szSize, const char* pcFile, int iLine);

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryI
{
    const void* (*add)       (const char* pcName, const void* pInterface);
    void        (*remove)    (const void* pInterface);
    void        (*replace)   (const void* pOldInterface, const void* pNewInterface);
    void        (*subscribe) (const void* pInterface, ptApiUpdateCallback ptCallback, void* pUserData);
    const void* (*first)     (const char* pcName);
    const void* (*next)      (const void* pPrev);
} plApiRegistryI;

typedef struct _plDataRegistryI
{
    // object creation & retrieval
    plDataID (*create_object)     (void);
    plDataID (*get_object_by_name)(const char* pcName); // assumes property 0 is name

    // future type system, current default type property 0 is a string (name) and property 1 is buffer (pointer)
    // plDataID (*create_object_of_type)(type));

    // reading (no locking or waiting)
    const plDataObject* (*read)      (plDataID);
    const char*         (*get_string)(const plDataObject*, uint32_t uProperty);
    void*               (*get_buffer)(const plDataObject*, uint32_t uProperty);
    void                (*end_read)  (const plDataObject*);

    // writing (global lock)
    plDataObject* (*write)     (plDataID);
    void          (*set_string)(plDataObject*, uint32_t uProperty, const char*);
    void          (*set_buffer)(plDataObject*, uint32_t uProperty, void*);
    void          (*commit)    (plDataObject*);

    // for convience, only use for infrequent operations (i.e. global extension data)
    void  (*set_data)(const char* pcName, void* pData);
    void* (*get_data)(const char* pcName);

    // called by backend
    void (*garbage_collect)(void);
} plDataRegistryI;

typedef struct _plExtensionRegistryI
{
    void (*reload)    (void);
    void (*unload_all)(void);
    void (*load)      (const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable);
    void (*unload)    (const char* pcName); 
} plExtensionRegistryI;

//-----------------------------------------------------------------------------
// [SECTION] structs (not for public use, subject to change)
//-----------------------------------------------------------------------------

typedef union _plDataID
{
    struct{
        uint32_t uSuperBlock : 10;
        uint32_t uBlock : 10;
        uint32_t uIndex : 10;
        uint64_t uUnused : 34;
    };
    uint64_t ulData;
} plDataID;

typedef struct _plAllocationEntry
{
    void*       pAddress;
    size_t      szSize;
    int         iLine;
    const char* pcFile; 
} plAllocationEntry;

typedef struct _plMemoryContext
{
  size_t                    szActiveAllocations;
  size_t                    szAllocationCount;
  size_t                    szAllocationFrees;
  plHashMap*                ptHashMap;
  plAllocationEntry*        sbtAllocations;
  size_t                    szMemoryUsage;
  const struct _plThreadsI* plThreadsI;
} plMemoryContext;

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef __cplusplus
    #if defined(_MSC_VER) //  Microsoft 
        #define PL_EXPORT extern "C" __declspec(dllexport)
    #elif defined(__GNUC__) //  GCC
        #define PL_EXPORT extern "C" __attribute__((visibility("default")))
    #else //  do nothing and hope for the best?
        #define PL_EXPORT
        #pragma warning Unknown dynamic link import/export semantics.
    #endif
#else

    #if defined(_MSC_VER) //  Microsoft 
        #define PL_EXPORT __declspec(dllexport)
    #elif defined(__GNUC__) //  GCC
        #define PL_EXPORT __attribute__((visibility("default")))
    #else //  do nothing and hope for the best?
        #define PL_EXPORT
        #pragma warning Unknown dynamic link import/export semantics.
    #endif
#endif

#ifdef PL_USER_CONFIG
#include PL_USER_CONFIG
#endif
#include "pl_config.h"

#ifdef PL_USE_STB_SPRINTF
#include "stb_sprintf.h"
#define pl_sprintf stbsp_sprintf
#define pl_vsprintf stbsp_vsprintf
#define pl_vnsprintf stbsp_vsnprintf
#else
#define pl_sprintf sprintf
#define pl_vsprintf vsprintf
#define pl_vnsprintf vsnprintf
#endif

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

#ifndef PL_ALLOC
    #include <stdlib.h>
    #define PL_ALLOC(x)      pl_realloc(NULL, x, __FILE__, __LINE__)
    #define PL_REALLOC(x, y) pl_realloc(x, y, __FILE__, __LINE__)
    #define PL_FREE(x)       pl_realloc(x, 0, __FILE__, __LINE__)
#endif

// pl_ds.h allocators (so they can be tracked)
#define PL_DS_ALLOC(x)                      pl_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) pl_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       pl_realloc((x), 0, __FILE__, __LINE__)

// settings
#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

#ifndef PL_MAX_PATH_LENGTH
    #define PL_MAX_PATH_LENGTH 1024
#endif

// log settings
#ifndef PL_GLOBAL_LOG_LEVEL
    #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL
#endif

#endif // PILOTLIGHT_H