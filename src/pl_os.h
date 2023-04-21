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

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api structs
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

#define PL_API_FILE        "FILE API"
#define PL_API_UDP         "UDP API"
#define PL_API_LIBRARY     "LIBRARY API"
#define PL_API_OS_SERVICES "OS SERVICES API"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// apis
typedef struct _plOsServicesApiI plOsServicesApiI;
typedef struct _plFileApiI       plFileApiI;
typedef struct _plLibraryApiI    plLibraryApiI;
typedef struct _plUdpApiI        plUdpApiI;

// types
typedef struct _plSharedLibrary plSharedLibrary;
typedef struct _plSocket        plSocket;

// external
typedef struct _plApiRegistryApiI plApiRegistryApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_load_file_api       (plApiRegistryApiI* ptApiRegistry);
void pl_load_udp_api        (plApiRegistryApiI* ptApiRegistry);
void pl_load_library_api    (plApiRegistryApiI* ptApiRegistry);
void pl_load_os_services_api(plApiRegistryApiI* ptApiRegistry);

//-----------------------------------------------------------------------------
// [SECTION] public structs
//-----------------------------------------------------------------------------

typedef struct _plFileApiI
{
  void (*read_file)(const char* pcFile, unsigned* puSize, char* pcBuffer, const char* pcMode);
  void (*copy_file)(const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer);
} plFileApiI;

typedef struct _plUdpApiI
{
  void (*create_udp_socket) (plSocket* ptSocketOut, bool bNonBlocking);
  void (*bind_udp_socket)   (plSocket* ptSocket, int iPort);
  bool (*send_udp_data)     (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
  bool (*get_udp_data)      (plSocket* ptSocket, void* pData, size_t szSize);
} plUdpApiI;

typedef struct _plLibraryApiI
{
  bool  (*has_library_changed)  (plSharedLibrary* ptLibrary);
  bool  (*load_library)         (plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile);
  void  (*reload_library)       (plSharedLibrary* ptLibrary);
  void* (*load_library_function)(plSharedLibrary* ptLibrary, const char* pcName);
} plLibraryApiI;

typedef struct _plOsServicesApiI
{
  int (*sleep)(uint32_t millisec);
} plOsServicesApiI;

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