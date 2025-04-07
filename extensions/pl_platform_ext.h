/*
   pl_platform_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public apis
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PLATFORM_EXT_H
#define PL_PLATFORM_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint8_t
#include <stdbool.h> // bool
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plAtomicsI_version       {1, 0, 0}
#define plFileI_version          {1, 0, 0}
#define plNetworkI_version       {1, 0, 0}
#define plThreadsI_version       {1, 0, 0}
#define plVirtualMemoryI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types (atomics ext)
typedef struct _plAtomicCounter plAtomicCounter; // opaque type

// basic types (network ext)
typedef struct _plSocket             plSocket;         // opaque type (used by platform backends)
typedef struct _plNetworkAddress     plNetworkAddress; // opaque type (used by platform backends)
typedef struct _plSocketReceiverInfo plSocketReceiverInfo;

// basic types (threads ext)
typedef struct _plThread            plThread;            // opaque type (used by platform backends)
typedef struct _plMutex             plMutex;             // opaque type (used by platform backends)
typedef struct _plCriticalSection   plCriticalSection;   // opaque type (used by platform backends)
typedef struct _plSemaphore         plSemaphore;         // opaque type (used by platform backends)
typedef struct _plBarrier           plBarrier;           // opaque type (used by platform backends)
typedef struct _plConditionVariable plConditionVariable; // opaque type (used by platform backends)
typedef struct _plThreadKey         plThreadKey;         // opaque type (used by platform backends)

typedef void* (*plThreadProcedure)(void*); // thread procedure signature

// enums (atomics ext)
typedef int plAtomicsResult; // -> enum _plAtomicsResult // Enum:

// enums (file ext)
typedef int plFileResult; // -> enum _plFileResult // Enum:

// enums (network ext)
typedef int plNetworkAddressFlags; // -> enum _plNetworkAddressFlags // Flags:
typedef int plSocketFlags;         // -> enum _plSocketFlags         // Flags:
typedef int plNetworkResult;       // -> enum _plNetworkResult       // Enum:

// enums (thread ext)
typedef int plThreadResult; // -> enum _plThreadResult // Enum:

//-----------------------------------------------------------------------------
// [SECTION] public apis
//-----------------------------------------------------------------------------

typedef struct _plAtomicsI
{

    plAtomicsResult (*create_atomic_counter)  (int64_t value, plAtomicCounter** counterPtrOut);
    void            (*destroy_atomic_counter) (plAtomicCounter**);
    void            (*atomic_store)           (plAtomicCounter*, int64_t value);
    int64_t         (*atomic_load)            (plAtomicCounter*);
    bool            (*atomic_compare_exchange)(plAtomicCounter*, int64_t expectedValue, int64_t desiredValue);
    int64_t         (*atomic_increment)       (plAtomicCounter*);
    int64_t         (*atomic_decrement)       (plAtomicCounter*);

} plAtomicsI;

typedef struct _plFileI
{

    // simple file ops
    bool         (*exists)(const char* path);
    plFileResult (*remove)(const char* path);
    plFileResult (*copy)  (const char* source, const char* destination);

    // binary files
    plFileResult (*binary_read) (const char* file, size_t* sizeOut, uint8_t* buffer); // pass NULL for buffer to get size
    plFileResult (*binary_write)(const char* file, size_t, uint8_t* buffer);

} plFileI;

typedef struct _plNetworkI
{

    // setup/shutdown
    bool (*initialize)(void);
    void (*cleanup)(void);

    // addresses
    plNetworkResult (*create_address) (const char* address, const char* service, plNetworkAddressFlags, plNetworkAddress** addressPtrOut);
    void            (*destroy_address)(plNetworkAddress**);

    // sockets: general
    void            (*create_socket) (plSocketFlags, plSocket** socketPtrOut);
    void            (*destroy_socket)(plSocket**);
    plNetworkResult (*bind_socket)   (plSocket*, plNetworkAddress*);
    
    // sockets: udp usually
    plNetworkResult (*send_socket_data_to) (plSocket*, plNetworkAddress*, const void* data, size_t, size_t* sentPtrSizeOut);
    plNetworkResult (*get_socket_data_from)(plSocket*, void* dataOut, size_t, size_t* recievedPtrSize, plSocketReceiverInfo*);

    // sockets: tcp usually
    plNetworkResult (*select_sockets)  (plSocket** sockets, bool* selectedSockets, uint32_t socketCount, uint32_t timeOutMilliSec);
    plNetworkResult (*connect_socket)  (plSocket*, plNetworkAddress*);
    plNetworkResult (*listen_socket)   (plSocket*);
    plNetworkResult (*accept_socket)   (plSocket*, plSocket** socketPtrOut);
    plNetworkResult (*get_socket_data) (plSocket*, void* dataOut, size_t, size_t* recievedPtrSize);
    plNetworkResult (*send_socket_data)(plSocket*, void* data, size_t, size_t* sentPtrSizeOut);

} plNetworkI;

typedef struct _plThreadsI
{

    // threads
    plThreadResult (*create_thread)            (plThreadProcedure, void* data, plThread** threadPtrOut);
    void           (*destroy_thread)           (plThread** threadPtr);
    void           (*join_thread)              (plThread*);
    uint64_t       (*get_thread_id)            (plThread*);
    void           (*yield_thread)             (void);
    void           (*sleep_thread)             (uint32_t milliSec);
    uint32_t       (*get_hardware_thread_count)(void);
    uint64_t       (*get_current_thread_id)    (void);

    // thread local storage
    plThreadResult (*allocate_thread_local_key) (plThreadKey** keyPtrOut);
    void           (*free_thread_local_key)     (plThreadKey** keyPtr);
    void*          (*allocate_thread_local_data)(plThreadKey*, size_t);
    void           (*free_thread_local_data)    (plThreadKey*, void* data);
    void*          (*get_thread_local_data)     (plThreadKey*);

    // mutexes
    plThreadResult (*create_mutex) (plMutex** mutexPtrOut);
    void           (*destroy_mutex)(plMutex** mutexPtr);
    void           (*lock_mutex)   (plMutex*);
    void           (*unlock_mutex) (plMutex*);

    // critical sections
    plThreadResult (*create_critical_section) (plCriticalSection** criticalSectionPtrOut);
    void           (*destroy_critical_section)(plCriticalSection**);
    void           (*enter_critical_section)  (plCriticalSection*);
    void           (*leave_critical_section)  (plCriticalSection*);

    // semaphores
    plThreadResult (*create_semaphore)     (uint32_t intialCount, plSemaphore** semaphorePtrOut);
    void           (*destroy_semaphore)    (plSemaphore**);
    void           (*wait_on_semaphore)    (plSemaphore*);
    bool           (*try_wait_on_semaphore)(plSemaphore*);
    void           (*release_semaphore)    (plSemaphore*);

    // barriers
    plThreadResult (*create_barrier) (uint32_t threadCount, plBarrier** barrierPtrOut);
    void           (*destroy_barrier)(plBarrier** barrierPtr);
    void           (*wait_on_barrier)(plBarrier*);

    // condition variables
    plThreadResult (*create_condition_variable)  (plConditionVariable** conditionVariablePtrOut);
    void           (*destroy_condition_variable) (plConditionVariable**);
    void           (*wake_condition_variable)    (plConditionVariable*);
    void           (*wake_all_condition_variable)(plConditionVariable*);
    void           (*sleep_condition_variable)   (plConditionVariable*, plCriticalSection*);

} plThreadsI;

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

enum _plAtomicsResult
{
    PL_ATOMICS_RESULT_FAIL    = 0,
    PL_ATOMICS_RESULT_SUCCESS = 1
};

enum _plFileResult
{
    PL_FILE_RESULT_FAIL    = 0,
    PL_FILE_RESULT_SUCCESS = 1
};

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

enum _plNetworkResult
{
    PL_NETWORK_RESULT_FAIL    = 0,
    PL_NETWORK_RESULT_SUCCESS = 1
};

enum _plThreadResult
{
    PL_THREAD_RESULT_FAIL    = 0,
    PL_THREAD_RESULT_SUCCESS = 1
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSocketReceiverInfo
{
    char acAddressBuffer[100];
    char acServiceBuffer[100];
} plSocketReceiverInfo;

#endif // PL_PLATFORM_EXT_H