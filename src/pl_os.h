/*
   pl_os.h
     * platform services
     * no dependencies
     * simple
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
*/

#ifndef PL_OS_H
#define PL_OS_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint32_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_NAME_LENGTH
#define PL_MAX_NAME_LENGTH 1024
#endif

#ifndef PL_DECLARE_STRUCT
#define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

PL_DECLARE_STRUCT(plSharedLibrary);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// files
void pl_read_file(const char* pcFile, unsigned* puSize, char* pcBuffer, const char* pcMode);
void pl_copy_file(const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer);

// shared libraries
bool  pl_has_library_changed  (plSharedLibrary* ptLibrary);
bool  pl_load_library         (plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile);
void  pl_reload_library       (plSharedLibrary* ptLibrary);
void* pl_load_library_function(plSharedLibrary* ptLibrary, const char* pcName);

// misc
int pl_sleep(uint32_t millisec);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSharedLibrary
{
    bool     bValid;
    uint32_t uTempIndex;
    char     acPath[PL_MAX_NAME_LENGTH];
    char     acTransitionalName[PL_MAX_NAME_LENGTH];
    char     acLockFile[PL_MAX_NAME_LENGTH];
    void*    _pPlatformData;
} plSharedLibrary;

#endif // PL_OS_H