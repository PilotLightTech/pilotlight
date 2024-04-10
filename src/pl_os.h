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

#define PL_API_WINDOW "PL_API_WINDOW"
typedef struct _plWindowI plWindowI;

#define PL_API_LIBRARY "PL_API_LIBRARY"
typedef struct _plLibraryApiI plLibraryApiI;

#define PL_API_FILE "FILE API"
typedef struct _plFileApiI plFileApiI;

#define PL_API_UDP "UDP API"
typedef struct _plUdpApiI plUdpApiI;

#define PL_API_THREADS "PL_API_THREADS"
typedef struct _plThreadsI plThreadsI;

#define PL_API_ATOMICS "PL_API_ATOMICS"
typedef struct _plAtomicsI plAtomicsI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// types
typedef struct _plWindow            plWindow;
typedef struct _plWindowDesc        plWindowDesc;
typedef struct _plSharedLibrary     plSharedLibrary;
typedef struct _plSocket            plSocket;
typedef struct _plThread            plThread;
typedef struct _plMutex             plMutex;
typedef struct _plCriticalSection   plCriticalSection;
typedef struct _plSemaphore         plSemaphore;
typedef struct _plBarrier           plBarrier;
typedef struct _plConditionVariable plConditionVariable;
typedef struct _plThreadKey         plThreadKey;
typedef struct _plAtomicCounter     plAtomicCounter;

// forward declarations
typedef void* (*plThreadProcedure)(void*);

// external
typedef struct _plApiRegistryI plApiRegistryI;

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plWindowI
{
    plWindow* (*create_window) (const plWindowDesc* ptDesc);
    void      (*destroy_window)(plWindow* ptWindow);
} plWindowI;

typedef struct _plLibraryApiI
{
    bool  (*load)         (const char* pcName, const char* pcTransitionalName, const char* pcLockFile, plSharedLibrary** pptLibraryOut);
    bool  (*has_changed)  (plSharedLibrary* ptLibrary);
    void  (*reload)       (plSharedLibrary* ptLibrary);
    void* (*load_function)(plSharedLibrary* ptLibrary, const char* pcName);
} plLibraryApiI;

typedef struct _plFileApiI
{
    void (*read)(const char* pcFile, uint32_t* puSize, char* pcBuffer, const char* pcMode);
    void (*copy)(const char* pcSource, const char* pcDestination);
} plFileApiI;

typedef struct _plUdpApiI
{
    void (*create_socket)(plSocket** pptSocketOut, bool bNonBlocking);
    void (*bind_socket)  (plSocket* ptSocket, int iPort);
    bool (*send_data)    (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
    bool (*get_data)     (plSocket* ptSocket, void* pData, size_t szSize);
} plUdpApiI;

typedef struct _plThreadsI
{

    // threads
    void (*create_thread)(plThreadProcedure ptProcedure, void* pData, plThread** ppThreadOut);
    void (*join_thread)  (plThread* ptThread);
    void (*yield_thread) (void);
    void (*sleep_thread) (uint32_t millisec);

    // thread local storage
    void  (*allocate_thread_local_key) (plThreadKey** pptKeyOut);
    void  (*free_thread_local_key)     (plThreadKey** ptKey);
    void* (*allocate_thread_local_data)(plThreadKey* ptKey, size_t szSize);
    void  (*free_thread_local_data)    (plThreadKey* ptKey, void* pData);
    void* (*get_thread_local_data)     (plThreadKey* ptKey);

    // mutexes
    void (*create_mutex) (plMutex** ppMutexOut);
    void (*destroy_mutex)(plMutex** ptMutex);
    void (*lock_mutex)   (plMutex* ptMutex);
    void (*unlock_mutex) (plMutex* ptMutex);

    // critical sections
    void (*create_critical_section) (plCriticalSection** pptCriticalSectionOut);
    void (*destroy_critical_section)(plCriticalSection** pptCriticalSection);
    void (*enter_critical_section)  (plCriticalSection* ptCriticalSection);
    void (*leave_critical_section)  (plCriticalSection* ptCriticalSection);

    // semaphores
    void (*create_semaphore)     (uint32_t uIntialCount, plSemaphore** pptSemaphoreOut);
    void (*destroy_semaphore)    (plSemaphore** pptSemaphore);
    void (*wait_on_semaphore)    (plSemaphore* ptSemaphore);
    bool (*try_wait_on_semaphore)(plSemaphore* ptSemaphore);
    void (*release_semaphore)    (plSemaphore* ptSemaphore);

    // barriers
    void (*create_barrier) (uint32_t uThreadCount, plBarrier** pptBarrierOut);
    void (*destroy_barrier)(plBarrier** pptBarrier);
    void (*wait_on_barrier)(plBarrier* ptBarrier);

    // condition variables
    void (*create_condition_variable)  (plConditionVariable** pptConditionVariableOut);
    void (*destroy_condition_variable) (plConditionVariable** pptConditionVariable);
    void (*wake_condition_variable)    (plConditionVariable* ptConditionVariable);
    void (*wake_all_condition_variable)(plConditionVariable* ptConditionVariable);
    void (*sleep_condition_variable)   (plConditionVariable* ptConditionVariable, plCriticalSection* ptCriticalSection);

    // misc.
    uint32_t (*get_hardware_thread_count)(void);
} plThreadsI;

typedef struct _plAtomicsI
{
    void    (*create_atomic_counter)  (int64_t ilValue, plAtomicCounter** pptCounter);
    void    (*destroy_atomic_counter) (plAtomicCounter** pptCounter);
    void    (*atomic_store)           (plAtomicCounter* ptCounter, int64_t ilValue);
    int64_t (*atomic_load)            (plAtomicCounter* ptCounter);
    bool    (*atomic_compare_exchange)(plAtomicCounter* ptCounter, int64_t ilExpectedValue, int64_t ilDesiredValue);
    void    (*atomic_increment)       (plAtomicCounter* ptCounter);
    void    (*atomic_decrement)       (plAtomicCounter* ptCounter);
} plAtomicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plWindowDesc
{
    const char* pcName;
    uint32_t    uWidth;
    uint32_t    uHeight;
    int         iXPos;
    int         iYPos;
} plWindowDesc;

typedef struct _plWindow
{
    plWindowDesc tDesc;
    void*        _pPlatformData;
} plWindow;

#endif // PL_OS_H