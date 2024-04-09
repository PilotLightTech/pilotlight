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

#define PL_API_THREADS "PL_API_THREADS"
typedef struct _plThreadsI plThreadsI;

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
typedef struct _plSocket        plSocket;
typedef struct _plThread        plThread;
typedef struct _plMutex         plMutex;
typedef struct _plSemaphore     plSemaphore;
typedef struct _plThreadKey     plThreadKey;

// forward declarations
typedef void* (*plThreadProcedure)(void*);

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

typedef struct _plThreadsI
{

  // general ops
  void     (*yield)(void);
  void     (*sleep)(uint32_t millisec);

  // threads
  plThread (*create)   (plThreadProcedure ptProcedure, void* pData);
  void     (*join)     (plThread* ptThread);
  void     (*terminate)(plThread* ptThread);

  // thread local storage
  plThreadKey (*allocate_thread_local_key) (void);
  void        (*free_thread_local_key)     (plThreadKey* ptKey);
  void*       (*allocate_thread_local_data)(plThreadKey* ptKey, size_t szSize);
  void        (*free_thread_local_data)    (plThreadKey* ptKey, void* pData);
  void*       (*get_thread_local_data)     (plThreadKey* ptKey);

  // mutexes
  plMutex (*create_mutex) (void);
  void    (*destroy_mutex)(plMutex* ptMutex);
  void    (*lock_mutex)   (plMutex* ptMutex);
  void    (*unlock_mutex) (plMutex* ptMutex);

  // semaphores
  plSemaphore (*create_semaphore) (uint32_t uIntialCount);
  void        (*destroy_semaphore)(plSemaphore* ptSemaphore);
  bool        (*wait_on_semaphore)(plSemaphore* ptSemaphore);
  void        (*release_semaphore)(plSemaphore* ptSemaphore);

  // misc.
  uint32_t (*get_hardware_thread_count)(void);
} plThreadsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plThread
{
  void* _pPlatformData;
} plThread;

typedef struct _plMutex
{
  void* _pPlatformData;
} plMutex;

typedef struct _plSemaphore
{
  void* _pPlatformData;
} plSemaphore;

typedef struct _plThreadKey
{
  void* _pPlatformData;
} plThreadKey;

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