/*
   pl_platform_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] public apis
// [SECTION] public api structs
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PLATFORM_EXT_H
#define PL_PLATFORM_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>  // uint8_t
#include <stdbool.h> // bool
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plAtomicsI_version       {2, 0, 0}
#define plFileI_version          {1, 1, 0}
#define plNetworkI_version       {1, 0, 0}
#define plThreadsI_version       {1, 0, 1}
#define plVirtualMemoryI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_PATH_LENGTH
    #define PL_MAX_PATH_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types (atomics)
typedef struct _plAtomicCounter plAtomicCounter; // opaque type

// basic types (file)
typedef struct _plDirectoryEntry plDirectoryEntry;
typedef struct _plDirectoryInfo  plDirectoryInfo;

// basic types (network)
typedef struct _plSocket             plSocket;         // opaque type (used by platform backends)
typedef struct _plNetworkAddress     plNetworkAddress; // opaque type (used by platform backends)
typedef struct _plSocketReceiverInfo plSocketReceiverInfo;

// basic types (threads)
typedef struct _plThread            plThread;            // opaque type (used by platform backends)
typedef struct _plMutex             plMutex;             // opaque type (used by platform backends)
typedef struct _plCriticalSection   plCriticalSection;   // opaque type (used by platform backends)
typedef struct _plSemaphore         plSemaphore;         // opaque type (used by platform backends)
typedef struct _plBarrier           plBarrier;           // opaque type (used by platform backends)
typedef struct _plConditionVariable plConditionVariable; // opaque type (used by platform backends)
typedef struct _plThreadKey         plThreadKey;         // opaque type (used by platform backends)

typedef void* (*plThreadProcedure)(void*); // thread procedure signature

// enums (atomics)
typedef int plAtomicsResult; // -> enum _plAtomicsResult // Enum:

// enums (file)
typedef int plFileResult;         // -> enum _plFileResult // Enum:
typedef int plDirectoryEntryType; // -> enum _plDirectoryEntryType // Enum:

// enums (network)
typedef int plNetworkAddressFlags; // -> enum _plNetworkAddressFlags // Flags:
typedef int plSocketFlags;         // -> enum _plSocketFlags         // Flags:
typedef int plNetworkResult;       // -> enum _plNetworkResult       // Enum:

// enums (thread)
typedef int plThreadResult; // -> enum _plThreadResult // Enum:

//-----------------------------------------------------------------------------
// [SECTION] public apis
//-----------------------------------------------------------------------------

// extension load/unload (must call directly if not using extension system)
PL_API void pl_load_platform_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_platform_ext(plApiRegistryI*, bool reload);

//-----------------------------atomics api-------------------------------------

PL_API plAtomicsResult pl_atomics_create_counter  (int64_t value, plAtomicCounter** counterPtrOut);
PL_API void            pl_atomics_destroy_counter (plAtomicCounter**);
PL_API void            pl_atomics_store           (plAtomicCounter*, int64_t value);
PL_API int64_t         pl_atomics_load            (plAtomicCounter*);
PL_API bool            pl_atomics_compare_exchange(plAtomicCounter*, int64_t expectedValue, int64_t desiredValue);
PL_API int64_t         pl_atomics_increment       (plAtomicCounter*);
PL_API int64_t         pl_atomics_decrement       (plAtomicCounter*);

//-----------------------------threads api-------------------------------------

// threads
PL_API plThreadResult pl_threads_create_thread            (plThreadProcedure, void* data, plThread** threadPtrOut);
PL_API void           pl_threads_destroy_thread           (plThread** threadPtr);
PL_API void           pl_threads_join_thread              (plThread*);
PL_API uint64_t       pl_threads_get_thread_id            (plThread*);
PL_API void           pl_threads_yield_thread             (void);
PL_API void           pl_threads_sleep_thread             (uint32_t milliSec);
PL_API uint32_t       pl_threads_get_hardware_thread_count(void);
PL_API uint64_t       pl_threads_get_current_thread_id    (void);

// thread local storage
PL_API plThreadResult pl_threads_allocate_thread_local_key (plThreadKey** keyPtrOut);
PL_API void           pl_threads_free_thread_local_key     (plThreadKey** keyPtr);
PL_API void*          pl_threads_allocate_thread_local_data(plThreadKey*, size_t);
PL_API void           pl_threads_free_thread_local_data    (plThreadKey*, void* data);
PL_API void*          pl_threads_get_thread_local_data     (plThreadKey*);

// mutexes
PL_API plThreadResult pl_threads_create_mutex (plMutex** mutexPtrOut);
PL_API void           pl_threads_destroy_mutex(plMutex** mutexPtr);
PL_API void           pl_threads_lock_mutex   (plMutex*);
PL_API void           pl_threads_unlock_mutex (plMutex*);

// critical sections
PL_API plThreadResult pl_threads_create_critical_section (plCriticalSection** criticalSectionPtrOut);
PL_API void           pl_threads_destroy_critical_section(plCriticalSection**);
PL_API void           pl_threads_enter_critical_section  (plCriticalSection*);
PL_API void           pl_threads_leave_critical_section  (plCriticalSection*);

// semaphores
PL_API plThreadResult pl_threads_create_semaphore     (uint32_t value, plSemaphore** semaphorePtrOut);
PL_API void           pl_threads_destroy_semaphore    (plSemaphore**);
PL_API void           pl_threads_wait_on_semaphore    (plSemaphore*); // waits until semaphore value is 0
PL_API bool           pl_threads_try_wait_on_semaphore(plSemaphore*);
PL_API void           pl_threads_release_semaphore    (plSemaphore*); // decrements semaphore value

// barriers
PL_API plThreadResult pl_threads_create_barrier (uint32_t threadCount, plBarrier** barrierPtrOut);
PL_API void           pl_threads_destroy_barrier(plBarrier** barrierPtr);
PL_API void           pl_threads_wait_on_barrier(plBarrier*);

// condition variables
PL_API plThreadResult pl_threads_create_condition_variable  (plConditionVariable** conditionVariablePtrOut);
PL_API void           pl_threads_destroy_condition_variable (plConditionVariable**);
PL_API void           pl_threads_wake_condition_variable    (plConditionVariable*);
PL_API void           pl_threads_wake_all_condition_variable(plConditionVariable*);
PL_API void           pl_threads_sleep_condition_variable   (plConditionVariable*, plCriticalSection*);

//-------------------------------file api--------------------------------------

// simple file ops
PL_API bool         pl_file_exists(const char* path);
PL_API plFileResult pl_file_remove(const char* path);
PL_API plFileResult pl_file_copy  (const char* source, const char* destination);

// binary files
PL_API plFileResult pl_file_binary_read (const char* file, size_t* sizeOut, uint8_t* buffer); // pass NULL for buffer to get size
PL_API plFileResult pl_file_binary_write(const char* file, size_t, uint8_t* buffer);

// simple directory ops
PL_API bool         pl_file_directory_exists(const char* path);
PL_API plFileResult pl_file_create_directory(const char* path);
PL_API plFileResult pl_file_remove_directory(const char* path);

// directory info
PL_API plFileResult pl_file_get_directory_info    (const char* path, plDirectoryInfo* infoOut);
PL_API void         pl_file_cleanup_directory_info(plDirectoryInfo* infoOut);

//-----------------------------network api-------------------------------------

// setup/shutdown
PL_API bool pl_network_initialize(void);
PL_API void pl_network_cleanup(void);

// addresses
PL_API plNetworkResult pl_network_create_address (const char* address, const char* service, plNetworkAddressFlags, plNetworkAddress** addressPtrOut);
PL_API void            pl_network_destroy_address(plNetworkAddress**);

// sockets: general
PL_API void            pl_network_create_socket (plSocketFlags, plSocket** socketPtrOut);
PL_API void            pl_network_destroy_socket(plSocket**);
PL_API plNetworkResult pl_network_bind_socket   (plSocket*, plNetworkAddress*);

// sockets: udp usually
PL_API plNetworkResult pl_network_send_socket_data_to (plSocket*, plNetworkAddress*, const void* data, size_t, size_t* sentPtrSizeOut);
PL_API plNetworkResult pl_network_get_socket_data_from(plSocket*, void* dataOut, size_t, size_t* recievedPtrSize, plSocketReceiverInfo*);

// sockets: tcp usually
PL_API plNetworkResult pl_network_select_sockets  (plSocket** sockets, bool* selectedSockets, uint32_t socketCount, uint32_t timeOutMilliSec);
PL_API plNetworkResult pl_network_connect_socket  (plSocket*, plNetworkAddress*);
PL_API plNetworkResult pl_network_listen_socket   (plSocket*);
PL_API plNetworkResult pl_network_accept_socket   (plSocket*, plSocket** socketPtrOut);
PL_API plNetworkResult pl_network_get_socket_data (plSocket*, void* dataOut, size_t, size_t* recievedPtrSize);
PL_API plNetworkResult pl_network_send_socket_data(plSocket*, void* data, size_t, size_t* sentPtrSizeOut);

//-------------------------virtual memory api----------------------------------

// Notes
//   - committed memory does not necessarily mean the memory has been mapped to physical
//     memory. This is happens when the memory is actually touched. Even so, on Windows
//     you can not commit more memmory then you have in your page file.
//   - uncommitted memory does not necessarily mean the memory will be immediately
//     evicted. It is up to the OS.

PL_API size_t pl_virtual_memory_get_page_size(void);                  // returns memory page size
PL_API void*  pl_virtual_memory_alloc        (void* address, size_t); // reserves & commits a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
PL_API void*  pl_virtual_memory_reserve      (void* address, size_t); // reserves a block of memory. pAddress is starting address or use NULL to have system choose. szSize must be a multiple of memory page size.
PL_API void*  pl_virtual_memory_commit       (void* address, size_t); // commits a block of reserved memory. szSize must be a multiple of memory page size.
PL_API void   pl_virtual_memory_uncommit     (void* address, size_t); // uncommits a block of committed memory.
PL_API void   pl_virtual_memory_free         (void* address, size_t); // frees a block of previously reserved/committed memory. Must be the starting address returned from "reserve()" or "alloc()"

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plAtomicsI
{

    plAtomicsResult (*create_counter)  (int64_t value, plAtomicCounter** counterPtrOut);
    void            (*destroy_counter) (plAtomicCounter**);
    void            (*store)           (plAtomicCounter*, int64_t value);
    int64_t         (*load)            (plAtomicCounter*);
    bool            (*compare_exchange)(plAtomicCounter*, int64_t expectedValue, int64_t desiredValue);
    int64_t         (*increment)       (plAtomicCounter*);
    int64_t         (*decrement)       (plAtomicCounter*);

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

    // simple directory ops
    bool         (*directory_exists)(const char* path);
    plFileResult (*create_directory)(const char* path);
    plFileResult (*remove_directory)(const char* path);

    // directory info
    plFileResult (*get_directory_info)    (const char* path, plDirectoryInfo* infoOut);
    void         (*cleanup_directory_info)(plDirectoryInfo* infoOut);

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
    plThreadResult (*create_semaphore)     (uint32_t value, plSemaphore** semaphorePtrOut);
    void           (*destroy_semaphore)    (plSemaphore**);
    void           (*wait_on_semaphore)    (plSemaphore*); // waits until semaphore value is 0
    bool           (*try_wait_on_semaphore)(plSemaphore*);
    void           (*release_semaphore)    (plSemaphore*); // decrements semaphore value

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
    PL_FILE_RESULT_SUCCESS = 1,
    
    PL_FILE_DIRECTORY_NOT_EMPTY     = 2,
    PL_FILE_DIRECTORY_ALREADY_EXIST = 3
};

enum _plDirectoryEntryType
{
    PL_DIRECTORY_ENTRY_TYPE_UNKNOWN = 0,
    PL_DIRECTORY_ENTRY_TYPE_DIRECTORY,
    PL_DIRECTORY_ENTRY_TYPE_FILE,
    
    // Linux/MacOS only
    PL_DIRECTORY_ENTRY_TYPE_PIPE,
    PL_DIRECTORY_ENTRY_TYPE_LINK,
    PL_DIRECTORY_ENTRY_TYPE_SOCKET,
    PL_DIRECTORY_ENTRY_TYPE_BLOCK_DEVICE,
    PL_DIRECTORY_ENTRY_TYPE_CHARACTER_DEVICE,
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

typedef struct _plDirectoryEntry
{
    plDirectoryEntryType tType;
    char                 acName[PL_MAX_PATH_LENGTH];
} plDirectoryEntry;

typedef struct _plDirectoryInfo
{
    uint32_t          uFileCount;
    uint32_t          uDirectoryCount;
    uint32_t          uEntryCount;
    plDirectoryEntry* sbtEntries;
} plDirectoryInfo;

#ifdef __cplusplus
}
#endif

#endif // PL_PLATFORM_EXT_H