/*
   pl_library_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_LIBRARY_EXT_H
#define PL_LIBRARY_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plLibraryI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plLibraryDesc   plLibraryDesc;
typedef struct _plSharedLibrary plSharedLibrary; // opaque type

// enums
typedef int plLibraryResult; // -> enum _plLibraryResult // Enum:

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plLibraryI
{

    plLibraryResult (*load)         (plLibraryDesc, plSharedLibrary** libraryPtrOut);
    bool            (*has_changed)  (plSharedLibrary*);
    void            (*reload)       (plSharedLibrary*);
    void*           (*load_function)(plSharedLibrary*, const char*);
    
} plLibraryI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plLibraryResult
{
    PL_LIBRARY_RESULT_FAIL    = 0,
    PL_LIBRARY_RESULT_SUCCESS = 1
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plLibraryDesc
{
    const char* pcName;             // name of library (without extension)
    const char* pcTransitionalName; // default: pcName + '_'
    const char* pcLockFile;         // default: "lock.tmp"
} plLibraryDesc;

#endif // PL_LIBRARY_EXT_H