/*
   pilotlight.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
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
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include "stb_sprintf.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define pl_sprintf stbsp_sprintf
#define pl_vsprintf stbsp_vsprintf
#define pl_vnsprintf stbsp_vsnprintf

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
#define PL_MAX_NAME_LENGTH 1024
#define PL_MAX_PATH_LENGTH 1024

// profile settings
#define PL_PROFILE_ON

// log settings
#define PL_LOG_ON
#ifndef PL_GLOBAL_LOG_LEVEL
    #define PL_GLOBAL_LOG_LEVEL PL_LOG_LEVEL_ALL
#endif
#define PL_LOG_ERROR_BOLD
#define PL_LOG_FATAL_BOLD
#define PL_LOG_TRACE_BG_COLOR PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_DEBUG_BG_COLOR PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_INFO_BG_COLOR  PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_WARN_BG_COLOR  PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_ERROR_BG_COLOR PL_LOG_BG_COLOR_CODE_BLACK
#define PL_LOG_FATAL_BG_COLOR PL_LOG_BG_COLOR_CODE_RED
#define PL_LOG_TRACE_FG_COLOR PL_LOG_FG_COLOR_CODE_GREEN
#define PL_LOG_DEBUG_FG_COLOR PL_LOG_FG_COLOR_CODE_CYAN
#define PL_LOG_INFO_FG_COLOR  PL_LOG_FG_COLOR_CODE_WHITE
#define PL_LOG_WARN_FG_COLOR  PL_LOG_FG_COLOR_CODE_YELLOW
#define PL_LOG_ERROR_FG_COLOR PL_LOG_FG_COLOR_CODE_RED
#define PL_LOG_FATAL_FG_COLOR PL_LOG_FG_COLOR_CODE_WHITE

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef void (*ptApiUpdateCallback)(void*, void*, void*);

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

plApiRegistryApiI* pl_load_core_apis  (void);
void               pl_unload_core_apis(void);

void              pl_set_memory_context(plMemoryContext* ptMemoryContext);
plMemoryContext*  pl_get_memory_context(void);
void*             pl_realloc           (void* pBuffer, size_t szSize, const char* pcFile, int iLine);

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryApiI
{
    void* (*add)        (const char* pcName, void* pInterface);
    void  (*remove)     (void* pInterface);
    void  (*replace)    (void* pOldInterface, void* pNewInterface);
    void  (*subscribe)  (void* pInterface, ptApiUpdateCallback ptCallback, void* pUserData);
    void* (*first)      (const char* pcName);
    void* (*next)       (void* pPrev);
} plApiRegistryApiI;

typedef struct _plDataRegistryApiI
{
   void  (*set_data)(const char* pcName, void* pData);
   void* (*get_data)(const char* pcName);
} plDataRegistryApiI;

typedef struct _plExtensionRegistryApiI
{
    void (*reload)          (plApiRegistryApiI* ptApiRegistry);
    void (*load)            (plApiRegistryApiI* ptApiRegistry, const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc, bool bReloadable);
    void (*unload)          (plApiRegistryApiI* ptApiRegistry, const char* pcName);
    void (*unload_all)      (plApiRegistryApiI* ptApiRegistry);
    void (*load_from_config)(plApiRegistryApiI* ptApiRegistry, const char* pcConfigFile);
    void (*load_from_file)  (plApiRegistryApiI* ptApiRegistry, const char* pcFile);
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