/*
    pl_internal.h
      * mostly forward declarations for functions defined
        in pl.c for use by platform backends
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] globals
// [SECTION] os declarations
// [SECTION] helper declarations
// [SECTION] helpers
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_INTERNAL_H
#define PL_INTERNAL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_os.h"

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// general
plSharedLibrary* gptAppLibrary = NULL;
void*            gpUserData    = NULL;
plIO*            gptIOCtx      = NULL;

#ifndef PL_HEADLESS_APP
plWindow** gsbtWindows = NULL;
#endif

// apis
const plDataRegistryI*      gptDataRegistry      = NULL;
const plApiRegistryI*       gptApiRegistry       = NULL;
const plExtensionRegistryI* gptExtensionRegistry = NULL;
const plIOI*                gptIOI               = NULL;
const plMemoryI*            gptMemory            = NULL;

// app function pointers
void* (*pl_app_load)    (const plApiRegistryI*, void*);
void  (*pl_app_shutdown)(void*);
void  (*pl_app_resize)  (void*);
void  (*pl_app_update)  (void*);
bool  (*pl_app_info)    (const plApiRegistryI*);

//-----------------------------------------------------------------------------
// [SECTION] os declarations
//-----------------------------------------------------------------------------

// window api
#ifndef PL_HEADLESS_APP
plOSResult pl_create_window (const plWindowDesc*, plWindow** pptWindowOut);
void       pl_destroy_window(plWindow*);
#endif

// clip board
const char* pl_get_clipboard_text(void* user_data_ctx);
void        pl_set_clipboard_text(void* pUnused, const char* text);

// file api
bool       pl_file_exists      (const char* pcFile);
plOSResult pl_file_delete      (const char* pcFile);
plOSResult pl_binary_read_file (const char* pcFile, size_t* pszSize, uint8_t* pcBuffer);
plOSResult pl_copy_file        (const char* pcSource, const char* pcDestination);
plOSResult pl_binary_write_file(const char* pcFile, size_t szSize, uint8_t* pcBuffer);

// network api: general
plOSResult pl_create_address      (const char* pcAddress, const char* pcService, plNetworkAddressFlags, plNetworkAddress** pptAddressOut);
void       pl_destroy_address     (plNetworkAddress**);
void       pl_create_socket       (plSocketFlags, plSocket** pptSocketOut);
void       pl_destroy_socket      (plSocket**);
plOSResult pl_bind_socket         (plSocket*, plNetworkAddress*);
plOSResult pl_select_sockets      (plSocket**, bool* abSelectedSockets, uint32_t uSocketCount, uint32_t uTimeOutMilliSec);
plOSResult pl_send_socket_data_to (plSocket*, plNetworkAddress*, const void* pData, size_t, size_t* pszSentSizeOut);
plOSResult pl_get_socket_data_from(plSocket*, void* pOutData, size_t, size_t* pszRecievedSize, plSocketReceiverInfo*);
plOSResult pl_connect_socket      (plSocket*, plNetworkAddress* ptAddress);
plOSResult pl_send_socket_data    (plSocket*, void* pData, size_t, size_t* pszSentSizeOut);
plOSResult pl_get_socket_data     (plSocket*, void* pData, size_t, size_t* pszRecievedSize);
plOSResult pl_accept_socket       (plSocket*, plSocket** pptSocketOut);
plOSResult pl_listen_socket       (plSocket*);

// library api
bool       pl_has_library_changed  (plSharedLibrary*);
plOSResult pl_load_library         (const plLibraryDesc*, plSharedLibrary** pptLibraryOut);
void       pl_reload_library       (plSharedLibrary*);
void*      pl_load_library_function(plSharedLibrary*, const char* pcName);

// thread api: misc
void     pl_sleep(uint32_t millisec);
uint32_t pl_get_hardware_thread_count(void);

// thread api: thread
plOSResult pl_create_thread (plThreadProcedure, void* pData, plThread** ppThreadOut);
void       pl_destroy_thread(plThread**);
void       pl_join_thread   (plThread*);
void       pl_yield_thread  (void);
uint32_t   pl_get_thread_id (plThread*);

// thread api: mutex
plOSResult pl_create_mutex (plMutex** ppMutexOut);
void       pl_lock_mutex   (plMutex*);
void       pl_unlock_mutex (plMutex*);
void       pl_destroy_mutex(plMutex**);

// thread api: critical section
plOSResult pl_create_critical_section (plCriticalSection** pptCriticalSectionOut);
void       pl_destroy_critical_section(plCriticalSection**);
void       pl_enter_critical_section  (plCriticalSection*);
void       pl_leave_critical_section  (plCriticalSection*);

// thread api: semaphore
plOSResult pl_create_semaphore     (uint32_t uIntialCount, plSemaphore** pptSemaphoreOut);
void       pl_wait_on_semaphore    (plSemaphore*);
bool       pl_try_wait_on_semaphore(plSemaphore*);
void       pl_release_semaphore    (plSemaphore*);
void       pl_destroy_semaphore    (plSemaphore**);

// thread api: thread local storage
plOSResult pl_allocate_thread_local_key (plThreadKey** pptKeyOut);
void       pl_free_thread_local_key     (plThreadKey**);
void*      pl_allocate_thread_local_data(plThreadKey*, size_t szSize);
void*      pl_get_thread_local_data     (plThreadKey*);
void       pl_free_thread_local_data    (plThreadKey*, void* pData);

// thread api: conditional variable
plOSResult pl_create_condition_variable  (plConditionVariable** pptConditionVariableOut);
void       pl_destroy_condition_variable (plConditionVariable**);
void       pl_wake_condition_variable    (plConditionVariable*);
void       pl_wake_all_condition_variable(plConditionVariable*);
void       pl_sleep_condition_variable   (plConditionVariable*, plCriticalSection*);

// thread api: barrier
plOSResult pl_create_barrier (uint32_t uThreadCount, plBarrier** pptBarrierOut);
void       pl_destroy_barrier(plBarrier**);
void       pl_wait_on_barrier(plBarrier*);

// atomics
plOSResult pl_create_atomic_counter  (int64_t ilValue, plAtomicCounter** ptCounter);
void       pl_destroy_atomic_counter (plAtomicCounter**);
void       pl_atomic_store           (plAtomicCounter*, int64_t ilValue);
int64_t    pl_atomic_load            (plAtomicCounter*);
bool       pl_atomic_compare_exchange(plAtomicCounter*, int64_t ilExpectedValue, int64_t ilDesiredValue);
int64_t    pl_atomic_increment       (plAtomicCounter*);
int64_t    pl_atomic_decrement       (plAtomicCounter*);

// virtual memory
size_t pl_get_page_size   (void);
void*  pl_virtual_alloc   (void* pAddress, size_t);
void*  pl_virtual_reserve (void* pAddress, size_t);
void*  pl_virtual_commit  (void* pAddress, size_t); 
void   pl_virtual_uncommit(void* pAddress, size_t);
void   pl_virtual_free    (void* pAddress, size_t);

//-----------------------------------------------------------------------------
// [SECTION] helper declarations
//-----------------------------------------------------------------------------

void pl__handle_extension_reloads(void);
void pl__unload_all_extensions(void);
void pl__load_core_apis(void);
void pl__unload_core_apis(void);
void pl__check_for_leaks(void);
void pl__garbage_collect_data_reg(void);
void pl__load_os_apis(void);

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

// memory helpers
#define PL_ALLOC(x) gptMemory->realloc(NULL, x, __FILE__, __LINE__)
#define PL_FREE(x)  gptMemory->realloc(x, 0, __FILE__, __LINE__)

#endif // PL_INTERNAL_H