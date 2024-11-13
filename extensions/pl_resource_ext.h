/*
   pl_resource_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
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

#define plResourceI_version (plVersion){0, 1, 0}

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
typedef union _plResourceHandle plResourceHandle;

// enums
typedef int plResourceLoadFlags;

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plResourceI
{
    // resource retrieval
    const void* (*get_file_data)(plResourceHandle tResourceHandle, size_t* pszDataSize);
    const void* (*get_buffer_data)(plResourceHandle tResourceHandle, size_t* pszDataSize);

    void (*set_buffer_data)(plResourceHandle tResourceHandle, size_t szDataSize, void* pData);
    
    //  resources
    plResourceHandle (*load_resource)     (const char* pcName, plResourceLoadFlags tFlags, uint8_t* puData, size_t szDataSize);
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
typedef union _plResourceHandle
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t ulData;
} plResourceHandle;
#endif // PL_RESOURCE_HANDLE_DEFINED

#endif // PL_RESOURCE_EXT_H