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

#ifndef PL_PILOTLIGHT_H
#define PL_PILOTLIGHT_H

#define PILOTLIGHT_VERSION    "0.3.0"
#define PILOTLIGHT_VERSION_NUM 000300

#if defined(_MSC_VER) //  Microsoft 
    #define PL_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) //  GCC
    #define PL_EXPORT __attribute__((visibility("default")))
#else //  do nothing and hope for the best?
    #define PL_EXPORT
    #pragma warning Unknown dynamic link import/export semantics.
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryApiI plApiRegistryApiI;

#define PL_API_DATA_REGISTRY "PL_API_DATA_REGISTRY"
typedef struct _plDataRegistryApiI plDataRegistryApiI;

#define PL_API_EXTENSION_REGISTRY "PL_API_EXTENSION_REGISTRY"
typedef struct _plExtensionRegistryApiI plExtensionRegistryApiI;

//-----------------------------------------------------------------------------
// [SECTION] contexts
//-----------------------------------------------------------------------------

#define PL_CONTEXT_MEMORY "PL_CONTEXT_MEMORY"

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include "stb_sprintf.h"

#ifdef PL_USER_CONFIG
#include PL_USER_CONFIG
#endif
#include "pl_config.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef PL_USE_STB_SPRINTF
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

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef void (*ptApiUpdateCallback)(const void*, const void*, void*);

// types
typedef struct _plSharedLibrary plSharedLibrary;
typedef struct _plSocket plSocket;
typedef struct _plMemoryContext plMemoryContext;
typedef struct _plAllocationEntry plAllocationEntry;

// external forward declarations
typedef struct _plHashMap plHashMap; // pl_ds.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plApiRegistryApiI* pl_load_core_apis  (void);
void                     pl_unload_core_apis(void);

void              pl_set_memory_context(plMemoryContext* ptMemoryContext);
plMemoryContext*  pl_get_memory_context(void);
void*             pl_realloc           (void* pBuffer, size_t szSize, const char* pcFile, int iLine);

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryApiI
{
    const void* (*add)       (const char* pcName, const void* pInterface);
    void        (*remove)    (const void* pInterface);
    void        (*replace)   (const void* pOldInterface, const void* pNewInterface);
    void        (*subscribe) (const void* pInterface, ptApiUpdateCallback ptCallback, void* pUserData);
    const void* (*first)     (const char* pcName);
    const void* (*next)      (const void* pPrev);
} plApiRegistryApiI;

typedef struct _plDataRegistryApiI
{
   void  (*set_data)(const char* pcName, void* pData);
   void* (*get_data)(const char* pcName);
} plDataRegistryApiI;

typedef struct _plExtensionRegistryApiI
{
    void (*reload)          (void);
    void (*unload_all)      (void);
    void (*load)            (const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable);
    void (*unload)          (const char* pcName); 
    void (*load_from_config)(const plApiRegistryApiI* ptApiRegistry, const char* pcConfigFile);
    void (*load_from_file)  (const plApiRegistryApiI* ptApiRegistry, const char* pcFile);
} plExtensionRegistryApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAllocationEntry
{
    void*       pAddress;
    size_t      szSize;
    int         iLine;
    const char* pcFile; 
} plAllocationEntry;

typedef struct _plMemoryContext
{
  size_t             szActiveAllocations;
  size_t             szAllocationCount;
  size_t             szAllocationFrees;
  plHashMap*         ptHashMap;
  plAllocationEntry* sbtAllocations;
}
plMemoryContext;

#endif // PL_PILOTLIGHT_H