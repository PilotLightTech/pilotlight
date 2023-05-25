/*
   pl_os.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_OS_H
#define PL_OS_H

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_PATH_LENGTH
#define PL_MAX_PATH_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_LIBRARY "PL_API_LIBRARY"
typedef struct _plLibraryApiI plLibraryApiI;

#define PL_API_FILE "FILE API"
typedef struct _plFileApiI plFileApiI;

#define PL_API_UDP "UDP API"
typedef struct _plUdpApiI plUdpApiI;

#define PL_API_OS_SERVICES "OS SERVICES API"
typedef struct _plOsServicesApiI plOsServicesApiI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// types
typedef struct _plSharedLibrary plSharedLibrary;
typedef struct _plSocket plSocket;

// external
typedef struct _plApiRegistryApiI plApiRegistryApiI;

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plLibraryApiI
{
  bool  (*has_changed)  (plSharedLibrary* ptLibrary);
  bool  (*load)         (plSharedLibrary* ptLibrary, const char* pcName, const char* pcTransitionalName, const char* pcLockFile);
  void  (*reload)       (plSharedLibrary* ptLibrary);
  void* (*load_function)(plSharedLibrary* ptLibrary, const char* pcName);
} plLibraryApiI;

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

typedef struct _plSharedLibrary
{
    bool     bValid;
    uint32_t uTempIndex;
    char     acPath[PL_MAX_PATH_LENGTH];
    char     acTransitionalName[PL_MAX_PATH_LENGTH];
    char     acLockFile[PL_MAX_PATH_LENGTH];
    void*    _pPlatformData;
} plSharedLibrary;

#endif // PL_OS_H