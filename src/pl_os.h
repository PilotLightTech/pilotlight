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
#define PL_API_OS_SERVICES "OS SERVICES API"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// apis
typedef struct _plOsServicesApiI plOsServicesApiI;
typedef struct _plFileApiI       plFileApiI;
typedef struct _plUdpApiI        plUdpApiI;

// types
typedef struct _plSocket plSocket;

// external
typedef struct _plApiRegistryApiI plApiRegistryApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_load_os_apis(plApiRegistryApiI* ptApiRegistry);

//-----------------------------------------------------------------------------
// [SECTION] public structs
//-----------------------------------------------------------------------------

typedef struct _plFileApiI
{
  void (*read)(const char* pcFile, unsigned* puSize, char* pcBuffer, const char* pcMode);
  void (*copy)(const char* pcSource, const char* pcDestination, unsigned* puSize, char* pcBuffer);
} plFileApiI;

typedef struct _plUdpApiI
{
  void (*create_socket) (plSocket* ptSocketOut, bool bNonBlocking);
  void (*bind_socket)   (plSocket* ptSocket, int iPort);
  bool (*send_data)     (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
  bool (*get_data)      (plSocket* ptSocket, void* pData, size_t szSize);
} plUdpApiI;

typedef struct _plOsServicesApiI
{
  int (*sleep) (uint32_t millisec);
} plOsServicesApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSocket
{
  int   iPort;
  void* _pPlatformData;
} plSocket;

#endif // PL_OS_H