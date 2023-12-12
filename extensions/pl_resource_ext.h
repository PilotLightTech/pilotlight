/*
   pl_resource_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RESOURCE_EXT_H
#define PL_RESOURCE_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_RESOURCE "PL_API_RESOURCE"
typedef struct _plResourceI plResourceI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stddef.h>  // size_t
#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plResourceHandle plResourceHandle;

// enums
typedef int plResourceLoadFlags;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plResourceI* pl_load_resource_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plResourceI
{
    // resource retrieval
    const char* (*get_file_data)(plResourceHandle tResourceHandle, size_t* pszDataSize);
    
    //  resources
    plResourceHandle (*load_resource)     (const char* pcName, plResourceLoadFlags tFlags, char* pcData, size_t szDataSize);
    bool             (*is_resource_loaded)(const char* pcName);
    bool             (*is_resource_valid) (plResourceHandle tResourceHandle);
    void             (*unload_resource)   (plResourceHandle tResourceHandle);
} plResourceI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plResourceLoadFlags
{
    PL_RESOURCE_LOAD_FLAG_NONE,
    PL_RESOURCE_LOAD_FLAG_RETAIN_DATA
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifndef PL_RESOURCE_HANDLE_DEFINED
#define PL_RESOURCE_HANDLE_DEFINED
typedef struct _plResourceHandle
{
    uint32_t uIndex;
    uint32_t uGeneration;
} plResourceHandle;
#endif // PL_RESOURCE_HANDLE_DEFINED

#endif // PL_RESOURCE_EXT_H