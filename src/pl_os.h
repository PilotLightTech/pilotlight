/*
   pl_os.h
     * platform services
     * no dependencies
     * simple
     * must include either:
       - pl_win32.h + pl_win32.c
       - pl_linux.h + pl_linux.c
       - pl_macos.h + pl_macos.m
*/

// library version
#define PL_OS_VERSION    "0.1.1"
#define PL_OS_VERSION_NUM 00101

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
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_PATH_LENGTH
    #define PL_MAX_PATH_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// types
typedef struct _plSharedLibrary plSharedLibrary;
typedef struct _plSocket        plSocket;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~file ops~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Notes
//   - API subject to change slightly
//   - additional error checks needs to be added

void   pl_read_file            (const char* pcFile, unsigned* puSize, char* pcBuffer, const char* pcMode);
void   pl_copy_file            (const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~UDP sockets~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void   pl_create_udp_socket    (plSocket* ptSocketOut, bool bNonBlocking);
void   pl_bind_udp_socket      (plSocket* ptSocket, int iPort);
bool   pl_send_udp_data        (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
bool   pl_get_udp_data         (plSocket* ptSocket, void* pData, size_t szSize);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shared libraries~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Notes
//   - API subject to change slightly
//   - additional error checks needs to be added

bool   pl_has_library_changed  (plSharedLibrary* ptLibrary);
bool   pl_load_library         (plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile);
void   pl_reload_library       (plSharedLibrary* ptLibrary);
void*  pl_load_library_function(plSharedLibrary* ptLibrary, const char* pcName);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~misc~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int pl_sleep(uint32_t millisec);

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

typedef struct _plSocket
{
  int   iPort;
  void* _pPlatformData;
} plSocket;

#endif // PL_OS_H