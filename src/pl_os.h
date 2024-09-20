/*
   pl_os.h
     - optional OS provided APIs
*/

/*
Index of this file:
// [SECTION] header mess
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
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_WINDOW "PL_API_WINDOW"
typedef struct _plWindowI plWindowI;

#define PL_API_LIBRARY "PL_API_LIBRARY"
typedef struct _plLibraryI plLibraryI;

#define PL_API_FILE "FILE API"
typedef struct _plFileI plFileI;

#define PL_API_UDP "UDP API"
typedef struct _plUdpI plUdpI;

#define PL_API_THREADS "PL_API_THREADS"
typedef struct _plThreadsI plThreadsI;

#define PL_API_ATOMICS "PL_API_ATOMICS"
typedef struct _plAtomicsI plAtomicsI;

#define PL_API_VIRTUAL_MEMORY "PL_API_VIRTUAL_MEMORY"
typedef struct _plVirtualMemoryI plVirtualMemoryI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// types
typedef struct _plWindow            plWindow;
typedef struct _plWindowDesc        plWindowDesc;
typedef struct _plSharedLibrary     plSharedLibrary;     // opaque type (used by platform backends)
typedef struct _plSocket            plSocket;            // opaque type (used by platform backends)
typedef struct _plThread            plThread;            // opaque type (used by platform backends)
typedef struct _plMutex             plMutex;             // opaque type (used by platform backends)
typedef struct _plCriticalSection   plCriticalSection;   // opaque type (used by platform backends)
typedef struct _plSemaphore         plSemaphore;         // opaque type (used by platform backends)
typedef struct _plBarrier           plBarrier;           // opaque type (used by platform backends)
typedef struct _plConditionVariable plConditionVariable; // opaque type (used by platform backends)
typedef struct _plThreadKey         plThreadKey;         // opaque type (used by platform backends)
typedef struct _plAtomicCounter     plAtomicCounter;     // opaque type (used by platform backends)

// forward declarations
typedef void* (*plThreadProcedure)(void*);

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plWindowI
{
    plWindow* (*create_window) (const plWindowDesc*);
    void      (*destroy_window)(plWindow*);
} plWindowI;

typedef struct _plLibraryI
{
    bool  (*load)         (const char* pcName, const char* pcTransitionalName, const char* pcLockFile, plSharedLibrary** pptLibraryOut);
    bool  (*has_changed)  (plSharedLibrary*);
    void  (*reload)       (plSharedLibrary*);
    void* (*load_function)(plSharedLibrary*, const char*);
} plLibraryI;

typedef struct _plFileI
{
    // simple file ops
    bool     (*exists)(const char* pcPath);
    void     (*delete)(const char* pcPath);
    void     (*copy)  (const char* pcSource, const char* pcDestination);

    // reading
    void (*binary_read)(const char* pcFile, size_t* pszSize, uint8_t* puBuffer);

    // writing
    void (*binary_write)(const char* pcFile, size_t szSize, uint8_t* puBuffer);
} plFileI;

typedef struct _plUdpI
{
    void (*create_socket)(plSocket** pptSocketOut, bool bNonBlocking);
    void (*bind_socket)  (plSocket*, int iPort);
    bool (*send_data)    (plSocket* ptFromSocket, const char* pcDestIP, int iDestPort, void* pData, size_t szSize);
    bool (*get_data)     (plSocket*, void* pData, size_t szSize);
} plUdpI;

typedef struct _plThreadsI
{

    // threads
    void (*create_thread)(plThreadProcedure, void* pData, plThread** ppThreadOut);
    void (*destroy_thread)(plThread** ppThread);
    void (*join_thread)  (plThread*);
    void (*yield_thread) (void);
    void (*sleep_thread) (uint32_t uMilliSec);

    // thread local storage
    void  (*allocate_thread_local_key) (plThreadKey** pptKeyOut);
    void  (*free_thread_local_key)     (plThreadKey** pptKey);
    void* (*allocate_thread_local_data)(plThreadKey*, size_t szSize);
    void  (*free_thread_local_data)    (plThreadKey*, void* pData);
    void* (*get_thread_local_data)     (plThreadKey*);

    // mutexes
    void (*create_mutex) (plMutex** ppMutexOut);
    void (*destroy_mutex)(plMutex** pptMutex);
    void (*lock_mutex)   (plMutex*);
    void (*unlock_mutex) (plMutex*);

    // critical sections
    void (*create_critical_section) (plCriticalSection** pptCriticalSectionOut);
    void (*destroy_critical_section)(plCriticalSection**);
    void (*enter_critical_section)  (plCriticalSection*);
    void (*leave_critical_section)  (plCriticalSection*);

    // semaphores
    void (*create_semaphore)     (uint32_t uIntialCount, plSemaphore** pptSemaphoreOut);
    void (*destroy_semaphore)    (plSemaphore**);
    void (*wait_on_semaphore)    (plSemaphore*);
    bool (*try_wait_on_semaphore)(plSemaphore*);
    void (*release_semaphore)    (plSemaphore*);

    // barriers
    void (*create_barrier) (uint32_t uThreadCount, plBarrier** pptBarrierOut);
    void (*destroy_barrier)(plBarrier** pptBarrier);
    void (*wait_on_barrier)(plBarrier*);

    // condition variables
    void (*create_condition_variable)  (plConditionVariable** pptConditionVariableOut);
    void (*destroy_condition_variable) (plConditionVariable**);
    void (*wake_condition_variable)    (plConditionVariable*);
    void (*wake_all_condition_variable)(plConditionVariable*);
    void (*sleep_condition_variable)   (plConditionVariable*, plCriticalSection*);

    // misc.
    uint32_t (*get_hardware_thread_count)(void);
} plThreadsI;

typedef struct _plAtomicsI
{
    void    (*create_atomic_counter)  (int64_t ilValue, plAtomicCounter** pptCounterOut);
    void    (*destroy_atomic_counter) (plAtomicCounter**);
    void    (*atomic_store)           (plAtomicCounter*, int64_t ilValue);
    int64_t (*atomic_load)            (plAtomicCounter*);
    bool    (*atomic_compare_exchange)(plAtomicCounter*, int64_t ilExpectedValue, int64_t ilDesiredValue);
    void    (*atomic_increment)       (plAtomicCounter*);
    void    (*atomic_decrement)       (plAtomicCounter*);
} plAtomicsI;

typedef struct _plVirtualMemoryI
{

    // Notes
    //   - API subject to change slightly
    //   - additional error checks needs to be added
    //   - committed memory does not necessarily mean the memory has been mapped to physical
    //     memory. This is happens when the memory is actually touched. Even so, on Windows
    //     you can not commit more memmory then you have in your page file.
    //   - uncommitted memory does not necessarily mean the memory will be immediately
    //     evicted. It is up to the OS.

    size_t (*get_page_size)(void);                   // returns memory page size
    void*  (*alloc)        (void* pAddress, size_t); // reserves & commits a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
    void*  (*reserve)      (void* pAddress, size_t); // reserves a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
    void*  (*commit)       (void* pAddress, size_t); // commits a block of reserved memory. szSize must be a multiple of memory page size.
    void   (*uncommit)     (void* pAddress, size_t); // uncommits a block of committed memory.
    void   (*free)         (void* pAddress, size_t); // frees a block of previously reserved/committed memory. Must be the starting address returned from "reserve()" or "alloc()"
} plVirtualMemoryI;

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