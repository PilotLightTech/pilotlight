/*
   pl_ext.h, v0.1 (WIP)
   * no dependencies
   * simple
   Do this:
        #define PL_EXT_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_EXT_IMPLEMENTATION
   #include "pl_ext.h"
*/

/*
Index of this file:
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_EXT_H
#define PL_EXT_H

#ifndef PL_DECLARE_STRUCT
#define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

#ifndef PL_EXTENSION_PATH_MAX_LENGTH
    #define PL_EXTENSION_PATH_MAX_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
PL_DECLARE_STRUCT(plExtension);
PL_DECLARE_STRUCT(plApi);
PL_DECLARE_STRUCT(plExtensionRegistry);


PL_DECLARE_STRUCT(plSharedLibrary);
PL_DECLARE_STRUCT(plDataRegistry);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// registry
void                 pl_initialize_extension_registry(plExtensionRegistry* ptRegistry);
void                 pl_cleanup_extension_registry   (void);
void                 pl_set_extension_registry       (plExtensionRegistry* ptRegistry);
plExtensionRegistry* pl_get_extension_registry       (void);
void                 pl_handle_extension_reloads     (void);

// extensions
plExtension*         pl_get_extension   (const char* pcname);
void                 pl_load_extension  (plExtension* ptExtension);
void                 pl_unload_extension(plExtension* ptExtension);
void*                pl_get_api         (plExtension* ptExtension, const char* pcApiName);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plApi
{
    const char* pcName;
    void*       pApi;
} plApi;

typedef struct _plExtension
{
    const char*  pcExtensionName;
    const char*  pcLibName;
    const char*  pcTransName;
    const char*  pcLockName;
    const char*  pcLoadFunc;
    const char*  pcUnloadFunc;
    plApi*       atApis;
    uint32_t     uApiCount;

    void (*pl_load)   (plDataRegistry* ptDataRegistry, plExtensionRegistry* ptExtensionRegistry, plExtension* ptExtension, bool bReload);
    void (*pl_unload) (plDataRegistry* ptDataRegistry, plExtensionRegistry* ptRegistry, plExtension* ptExtension);
} plExtension;

typedef struct _plExtensionRegistry
{
    plExtension*      sbtExtensions;
    plSharedLibrary*  sbtLibs;
    plSharedLibrary** sbtHotLibs;
} plExtensionRegistry;

#endif // PL_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] includes
// [SECTION] global context
// [SECTION] public api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_registry.h"
#include "pl_ds.h"
#include "pl_os.h"

#ifdef PL_EXT_IMPLEMENTATION

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

plExtensionRegistry* gptExtRegistry = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_extension_registry(plExtensionRegistry* ptRegistry)
{
    gptExtRegistry = ptRegistry;
}

void
pl_cleanup_extension_registry(void)
{
    PL_ASSERT(gptExtRegistry && "global extension registry not set");
    pl_sb_free(gptExtRegistry->sbtExtensions);
    pl_sb_free(gptExtRegistry->sbtHotLibs);
    pl_sb_free(gptExtRegistry->sbtLibs);
}

void
pl_set_extension_registry(plExtensionRegistry* ptRegistry)
{
    gptExtRegistry = ptRegistry;
}

plExtensionRegistry*
pl_get_extension_registry(void)
{
    return gptExtRegistry;
}

plExtension*
pl_get_extension(const char* pcname)
{
    PL_ASSERT(gptExtRegistry && "global extension registry not set");

    for(uint32_t i = 0; i < pl_sb_size(gptExtRegistry->sbtExtensions); i++)
    {
        if(strcmp(pcname, gptExtRegistry->sbtExtensions[i].pcExtensionName) == 0)
            return &gptExtRegistry->sbtExtensions[i];
    }
    return NULL;
}

void
pl_load_extension(plExtension* ptExtension)
{
    PL_ASSERT(gptExtRegistry && "global extension registry not set");

    // check if extension exists already
    for(uint32_t i = 0; i < pl_sb_size(gptExtRegistry->sbtLibs); i++)
    {
        if(strcmp(ptExtension->pcLibName, gptExtRegistry->sbtLibs[i].acPath) == 0)
            return;
    }

    plSharedLibrary tLibrary = {0};

    if(pl_load_library(&tLibrary, ptExtension->pcLibName, ptExtension->pcTransName, ptExtension->pcLockName))
    {
        #ifdef _WIN32
            ptExtension->pl_load   = (void (__cdecl *)(plDataRegistry*, plExtensionRegistry*, plExtension*, bool)) pl_load_library_function(&tLibrary, ptExtension->pcLoadFunc);
            ptExtension->pl_unload = (void (__cdecl *)(plDataRegistry*, plExtensionRegistry*, plExtension*))       pl_load_library_function(&tLibrary, ptExtension->pcUnloadFunc);
        #else // linux
            ptExtension->pl_load   = (void (__attribute__(()) *)(plDataRegistry*, plExtensionRegistry*, plExtension*, bool)) pl_load_library_function(&tLibrary, ptExtension->pcLoadFunc);
            ptExtension->pl_unload = (void (__attribute__(()) *)(plDataRegistry*, plExtensionRegistry*, plExtension*))       pl_load_library_function(&tLibrary, ptExtension->pcUnloadFunc);
        #endif

        PL_ASSERT(ptExtension->pl_load);
        PL_ASSERT(ptExtension->pl_unload);
        pl_sb_push(gptExtRegistry->sbtLibs, tLibrary);
        ptExtension->pl_load(pl_get_data_registry(), gptExtRegistry, ptExtension, false);
        pl_sb_push(gptExtRegistry->sbtExtensions, *ptExtension);
    }
    else
    {
        PL_ASSERT(false && "extension not loaded");
    }
}

void
pl_unload_extension(plExtension* ptExtension)
{
    PL_ASSERT(gptExtRegistry && "global extension registry not set");

    for(uint32_t i = 0; i < pl_sb_size(gptExtRegistry->sbtExtensions); i++)
    {
        if(strcmp(ptExtension->pcExtensionName, gptExtRegistry->sbtExtensions[i].pcExtensionName) == 0)
        {
            gptExtRegistry->sbtExtensions[i].pl_unload(pl_get_data_registry(), gptExtRegistry, ptExtension);
            pl_sb_del_swap(gptExtRegistry->sbtExtensions, i);
            pl_sb_del_swap(gptExtRegistry->sbtLibs, i);
            return;
        }
    }

    PL_ASSERT(false && "extension not found");
}

void*
pl_get_api(plExtension* ptExtension, const char* pcApiName)
{
    PL_ASSERT(gptExtRegistry && "global extension registry not set");

    for(uint32_t i = 0; i < ptExtension->uApiCount; i++)
    {
        if(strcmp(ptExtension->atApis[i].pcName, pcApiName) == 0)
        {
            return ptExtension->atApis[i].pApi;
        }
    }
    PL_ASSERT(false && "api not found");
    return NULL;
}

void
pl_handle_extension_reloads(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gptExtRegistry->sbtLibs); i++)
    {
        if(pl_has_library_changed(&gptExtRegistry->sbtLibs[i]))
            pl_sb_push(gptExtRegistry->sbtHotLibs, &gptExtRegistry->sbtLibs[i]);
    }

    for(uint32_t i = 0; i < pl_sb_size(gptExtRegistry->sbtHotLibs); i++)
    {
       plSharedLibrary* ptLibrary = gptExtRegistry->sbtHotLibs[i];
       plExtension* ptExtension = &gptExtRegistry->sbtExtensions[i];
       pl_reload_library(ptLibrary);
        #ifdef _WIN32
            ptExtension->pl_load   = (void (__cdecl *)(plDataRegistry*, plExtensionRegistry*, plExtension*, bool)) pl_load_library_function(ptLibrary, ptExtension->pcLoadFunc);
            ptExtension->pl_unload = (void (__cdecl *)(plDataRegistry*, plExtensionRegistry*, plExtension*))       pl_load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
        #else // linux
            ptExtension->pl_load   = (void (__attribute__(()) *)(plDataRegistry*, plExtensionRegistry*, plExtension*, bool)) pl_load_library_function(ptLibrary, ptExtension->pcLoadFunc);
            ptExtension->pl_unload = (void (__attribute__(()) *)(plDataRegistry*, plExtensionRegistry*, plExtension*))       pl_load_library_function(ptLibrary, ptExtension->pcUnloadFunc);
        #endif

        PL_ASSERT(ptExtension->pl_load);
        PL_ASSERT(ptExtension->pl_unload);

        ptExtension->pl_load(pl_get_data_registry(), gptExtRegistry, ptExtension, true);
    }
    pl_sb_reset(gptExtRegistry->sbtHotLibs);
}

#endif // PL_EXT_IMPLEMENTATION