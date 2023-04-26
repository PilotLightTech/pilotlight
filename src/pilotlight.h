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

#define PL_API_MEMORY "PL_API_MEMORY"
typedef struct _plMemoryApiI plMemoryApiI;

#define PL_API_LIBRARY "PL_API_LIBRARY"
typedef struct _plLibraryApiI plLibraryApiI;

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

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

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

// external apis
typedef struct _plLibraryApiI plLibraryApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plApiRegistryApiI* pl_load_core_apis  (void);
void               pl_unload_core_apis(void);

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryApiI
{
    void  (*add)        (const char* pcName, void* pInterface);
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
    void (*load)            (plApiRegistryApiI* ptApiRegistry, const char* pcName, const char* pcLoadFunc, const char* pcUnloadFunc);
    void (*unload)          (plApiRegistryApiI* ptApiRegistry, const char* pcName);
    void (*load_from_config)(plApiRegistryApiI* ptApiRegistry, const char* pcConfigFile);
    void (*load_from_file)  (plApiRegistryApiI* ptApiRegistry, const char* pcFile);
} plExtensionRegistryApiI;

typedef struct _plMemoryApiI
{
    void* (*alloc)(size_t szSize);
    void  (*free) (void* pBuffer);
} plMemoryApiI;

typedef struct _plLibraryApiI
{
  bool  (*has_changed)  (plSharedLibrary* ptLibrary);
  bool  (*load)         (plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile);
  void  (*reload)       (plSharedLibrary* ptLibrary);
  void* (*load_function)(plSharedLibrary* ptLibrary, const char* pcName);
} plLibraryApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSharedLibrary
{
    bool     bValid;
    uint32_t uTempIndex;
    char     acPath[PL_MAX_PATH_LENGTH];
    char     acTransitionalName[PL_MAX_PATH_LENGTH];
    char     acLockFile[PL_MAX_PATH_LENGTH];
    void*    _pPlatformData;
} plSharedLibrary;

#endif // PL_PILOTLIGHT_H