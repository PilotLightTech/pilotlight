/*
   pl_os.h
     * platform services
     * no dependencies
     * simple
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
*/

#ifndef PL_OS_H
#define PL_OS_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#include "pl.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

PL_DECLARE_STRUCT(plSharedLibrary);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// files
void pl_read_file(const char* file, unsigned* size, char* buffer, const char* mode);
void pl_copy_file(const char* source, const char* destination, unsigned* size, char* buffer);

// shared libraries
bool  pl_has_library_changed  (plSharedLibrary* library);
bool  pl_load_library         (plSharedLibrary* library, const char* name, const char* transitionalName, const char* lockFile);
void  pl_reload_library       (plSharedLibrary* library);
void* pl_load_library_function(plSharedLibrary* library, const char* name);

// misc
int pl_sleep(uint32_t millisec);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plSharedLibrary_t
{
    bool     valid;
    uint32_t tempIndex;
    char     path[PL_MAX_NAME_LENGTH];
    char     transitionalName[PL_MAX_NAME_LENGTH];
    char     lockFile[PL_MAX_NAME_LENGTH];
    void*    _platformData;
} plSharedLibrary;

#endif // PL_OS_H