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
// [SECTION] enums
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

#define PL_API_FILE "PL_API_FILE"
typedef struct _plFileI plFileI;

#define PL_API_NETWORK "PL_API_NETWORK"
typedef struct _plNetworkI plNetworkI;

#define PL_API_THREADS "PL_API_THREADS"
typedef struct _plThreadsI plThreadsI;

#define PL_API_ATOMICS "PL_API_ATOMICS"
typedef struct _plAtomicsI plAtomicsI;

#define PL_API_VIRTUAL_MEMORY "PL_API_VIRTUAL_MEMORY"
typedef struct _plVirtualMemoryI plVirtualMemoryI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint32_t, int64_t
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// types
typedef struct _plWindow             plWindow;
typedef struct _plWindowDesc         plWindowDesc;
typedef struct _plLibraryDesc        plLibraryDesc;
typedef struct _plSocketReceiverInfo plSocketReceiverInfo;
typedef struct _plSharedLibrary      plSharedLibrary;     // opaque type (used by platform backends)
typedef struct _plSocket             plSocket;            // opaque type (used by platform backends)
typedef struct _plNetworkAddress     plNetworkAddress;    // opaque type (used by platform backends)
typedef struct _plThread             plThread;            // opaque type (used by platform backends)
typedef struct _plMutex              plMutex;             // opaque type (used by platform backends)
typedef struct _plCriticalSection    plCriticalSection;   // opaque type (used by platform backends)
typedef struct _plSemaphore          plSemaphore;         // opaque type (used by platform backends)
typedef struct _plBarrier            plBarrier;           // opaque type (used by platform backends)
typedef struct _plConditionVariable  plConditionVariable; // opaque type (used by platform backends)
typedef struct _plThreadKey          plThreadKey;         // opaque type (used by platform backends)
typedef struct _plAtomicCounter      plAtomicCounter;     // opaque type (used by platform backends)

// enums
typedef int plNetworkAddressFlags; // -> enum _plNetworkAddressFlags // Flags:
typedef int plSocketFlags;         // -> enum _plSocketFlags         // Flags:
typedef int plOSResult;            // -> enum _plOSResult            // Enum:

// thread procedure signature
typedef void* (*plThreadProcedure)(void*);

//-----------------------------------------------------------------------------
// [SECTION] api structs
//-----------------------------------------------------------------------------

typedef struct _plWindowI
{

    plOSResult (*create_window) (const plWindowDesc*, plWindow** windowPtrOut);
    void       (*destroy_window)(plWindow*);
    
} plWindowI;

typedef struct _plLibraryI
{

    plOSResult (*load)         (const plLibraryDesc*, plSharedLibrary** libraryPtrOut);
    bool       (*has_changed)  (plSharedLibrary*);
    void       (*reload)       (plSharedLibrary*);
    void*      (*load_function)(plSharedLibrary*, const char*);
    
} plLibraryI;

typedef struct _plFileI
{

    // simple file ops
    bool       (*exists)(const char* path);
    plOSResult (*delete)(const char* path);
    plOSResult (*copy)  (const char* source, const char* destination);

    // binary files
    plOSResult (*binary_read) (const char* file, size_t* sizeOut, uint8_t* buffer); // pass NULL for puBuffer to get size
    plOSResult (*binary_write)(const char* file, size_t, uint8_t* buffer);

} plFileI;

typedef struct _plNetworkI
{

    // addresses
    plOSResult (*create_address) (const char* address, const char* service, plNetworkAddressFlags, plNetworkAddress** addressPtrOut);
    void       (*destroy_address)(plNetworkAddress**);

    // sockets: general
    void       (*create_socket) (plSocketFlags, plSocket** socketPtrOut);
    void       (*destroy_socket)(plSocket**);
    plOSResult (*bind_socket)   (plSocket*, plNetworkAddress*);
    
    // sockets: udp usually
    plOSResult (*send_socket_data_to) (plSocket*, plNetworkAddress*, const void* data, size_t, size_t* sentPtrSizeOut);
    plOSResult (*get_socket_data_from)(plSocket*, void* dataOut, size_t, size_t* recievedPtrSize, plSocketReceiverInfo*);

    // sockets: tcp usually
    plOSResult (*select_sockets)  (plSocket** sockets, bool* selectedSockets, uint32_t socketCount, uint32_t timeOutMilliSec);
    plOSResult (*connect_socket)  (plSocket*, plNetworkAddress*);
    plOSResult (*listen_socket)   (plSocket*);
    plOSResult (*accept_socket)   (plSocket*, plSocket** socketPtrOut);
    plOSResult (*get_socket_data) (plSocket*, void* dataOut, size_t, size_t* recievedPtrSize);
    plOSResult (*send_socket_data)(plSocket*, void* data, size_t, size_t* sentPtrSizeOut);

} plNetworkI;

typedef struct _plThreadsI
{

    // threads
    plOSResult (*create_thread) (plThreadProcedure, void* data, plThread** threadPtrOut);
    void       (*destroy_thread)(plThread** threadPtr);
    void       (*join_thread)   (plThread*);
    uint32_t   (*get_thread_id) (plThread*);
    void       (*yield_thread)  (void);
    void       (*sleep_thread)  (uint32_t milliSec);
    uint32_t   (*get_hardware_thread_count)(void);

    // thread local storage
    plOSResult (*allocate_thread_local_key) (plThreadKey** keyPtrOut);
    void       (*free_thread_local_key)     (plThreadKey** keyPtr);
    void*      (*allocate_thread_local_data)(plThreadKey*, size_t);
    void       (*free_thread_local_data)    (plThreadKey*, void* data);
    void*      (*get_thread_local_data)     (plThreadKey*);

    // mutexes
    plOSResult (*create_mutex) (plMutex** mutexPtrOut);
    void       (*destroy_mutex)(plMutex** mutexPtr);
    void       (*lock_mutex)   (plMutex*);
    void       (*unlock_mutex) (plMutex*);

    // critical sections
    plOSResult (*create_critical_section) (plCriticalSection** criticalSectionPtrOut);
    void       (*destroy_critical_section)(plCriticalSection**);
    void       (*enter_critical_section)  (plCriticalSection*);
    void       (*leave_critical_section)  (plCriticalSection*);

    // semaphores
    plOSResult (*create_semaphore)     (uint32_t intialCount, plSemaphore** semaphorePtrOut);
    void       (*destroy_semaphore)    (plSemaphore**);
    void       (*wait_on_semaphore)    (plSemaphore*);
    bool       (*try_wait_on_semaphore)(plSemaphore*);
    void       (*release_semaphore)    (plSemaphore*);

    // barriers
    plOSResult (*create_barrier) (uint32_t threadCount, plBarrier** barrierPtrOut);
    void       (*destroy_barrier)(plBarrier** barrierPtr);
    void       (*wait_on_barrier)(plBarrier*);

    // condition variables
    plOSResult (*create_condition_variable)  (plConditionVariable** conditionVariablePtrOut);
    void       (*destroy_condition_variable) (plConditionVariable**);
    void       (*wake_condition_variable)    (plConditionVariable*);
    void       (*wake_all_condition_variable)(plConditionVariable*);
    void       (*sleep_condition_variable)   (plConditionVariable*, plCriticalSection*);

} plThreadsI;

typedef struct _plAtomicsI
{

    plOSResult (*create_atomic_counter)  (int64_t value, plAtomicCounter** counterPtrOut);
    void       (*destroy_atomic_counter) (plAtomicCounter**);
    void       (*atomic_store)           (plAtomicCounter*, int64_t value);
    int64_t    (*atomic_load)            (plAtomicCounter*);
    bool       (*atomic_compare_exchange)(plAtomicCounter*, int64_t expectedValue, int64_t desiredValue);
    int64_t    (*atomic_increment)       (plAtomicCounter*);
    int64_t    (*atomic_decrement)       (plAtomicCounter*);

} plAtomicsI;

typedef struct _plVirtualMemoryI
{

    // Notes
    //   - committed memory does not necessarily mean the memory has been mapped to physical
    //     memory. This is happens when the memory is actually touched. Even so, on Windows
    //     you can not commit more memmory then you have in your page file.
    //   - uncommitted memory does not necessarily mean the memory will be immediately
    //     evicted. It is up to the OS.

    size_t (*get_page_size)(void);                  // returns memory page size
    void*  (*alloc)        (void* address, size_t); // reserves & commits a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
    void*  (*reserve)      (void* address, size_t); // reserves a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
    void*  (*commit)       (void* address, size_t); // commits a block of reserved memory. szSize must be a multiple of memory page size.
    void   (*uncommit)     (void* address, size_t); // uncommits a block of committed memory.
    void   (*free)         (void* address, size_t); // frees a block of previously reserved/committed memory. Must be the starting address returned from "reserve()" or "alloc()"
    
} plVirtualMemoryI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plNetworkAddressFlags
{
    PL_NETWORK_ADDRESS_FLAGS_NONE = 0,
    PL_NETWORK_ADDRESS_FLAGS_IPV4 = 1 << 0,
    PL_NETWORK_ADDRESS_FLAGS_IPV6 = 1 << 1,
    PL_NETWORK_ADDRESS_FLAGS_UDP  = 1 << 2,
    PL_NETWORK_ADDRESS_FLAGS_TCP  = 1 << 3,
};

enum _plSocketFlags
{
    PL_SOCKET_FLAGS_NONE         = 0,
    PL_SOCKET_FLAGS_NON_BLOCKING = 1 << 0,
};

enum _plOSResult
{
    PL_OS_RESULT_FAIL    = 0,
    PL_OS_RESULT_SUCCESS = 1
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSocketReceiverInfo
{
    char acAddressBuffer[100];
    char acServiceBuffer[100];
} plSocketReceiverInfo;

typedef struct _plWindowDesc
{
    const char* pcName;
    uint32_t    uWidth;
    uint32_t    uHeight;
    int         iXPos;
    int         iYPos;
    const void* pNext;
} plWindowDesc;

typedef struct _plWindow
{
    plWindowDesc tDesc;
    void*        _pPlatformData;
} plWindow;

typedef struct _plLibraryDesc
{
    const char* pcName;             // name of library (without extension)
    const char* pcTransitionalName; // default: pcName + '_'
    const char* pcLockFile;         // default: "lock.tmp"
} plLibraryDesc;

#endif // PL_OS_H