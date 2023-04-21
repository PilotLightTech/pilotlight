/*
   pl_registry.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_REGISTRY_H
#define PL_REGISTRY_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_API_DATA_REGISTRY      "DATA REGISTRY API"
#define PL_API_EXTENSION_REGISTRY "EXT REG API"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plExtension plExtension;
typedef void (*ptApiUpdateCallback)(void*, void*);

// apis
typedef struct _plDataRegistryApiI      plDataRegistryApiI;
typedef struct _plApiRegistryApiI       plApiRegistryApiI;
typedef struct _plExtensionRegistryApiI plExtensionRegistryApiI;

// external
typedef struct _plLibraryApiI plLibraryApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plApiRegistryApiI*       pl_load_api_registry        (void);
void                     pl_unload_api_registry      (void);
void                     pl_load_data_registry_api   (plApiRegistryApiI* ptApiRegistry);
void                     pl_unload_data_registry_api (void);
void                     pl_load_extension_registry  (plApiRegistryApiI* ptApiRegistry);
void                     pl_unload_extension_registry(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDataRegistryApiI
{
   void  (*set_data)(const char* pcName, void* pData);
   void* (*get_data)(const char* pcName);
} plDataRegistryApiI;

typedef struct _plApiRegistryApiI
{
    void  (*add)        (const char* pcName, void* pInterface);
    void  (*remove)     (void* pInterface);
    void  (*replace)    (void* pOldInterface, void* pNewInterface);
    void  (*subscribe)  (void* pInterface, ptApiUpdateCallback ptCallback, void* pUserData);
    void* (*first)      (const char* pcName);
    void* (*next)       (void* pPrev);
} plApiRegistryApiI;

typedef struct _plExtensionRegistryApiI
{
    void (*reload)(plApiRegistryApiI* ptApiRegistry, plLibraryApiI* ptLibraryApi);
    void (*load)  (plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension);
    void (*unload)(plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension);
} plExtensionRegistryApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plExtension
{
    const char*  pcLibName;
    const char*  pcTransName;
    const char*  pcLoadFunc;
    const char*  pcUnloadFunc;

    void (*pl_load)   (plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension, bool bReload);
    void (*pl_unload) (plApiRegistryApiI* ptApiRegistry, plExtension* ptExtension);
} plExtension;

#endif // PL_REGISTRY_H