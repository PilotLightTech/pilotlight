/*
    pl_os_ext_internal.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] globals
// [SECTION] os declarations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_OS_EXT_INTERNAL_H
#define PL_OS_EXT_INTERNAL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_network_ext.h"
#include "pl_threads_ext.h"
#include "pl_atomics_ext.h"
#include "pl_virtual_memory_ext.h"

#include "pl.h"

//-----------------------------------------------------------------------------
// [SECTION] globals
//-----------------------------------------------------------------------------

// apis
const plMemoryI* gptMemory = NULL;

//-----------------------------------------------------------------------------
// [SECTION] os declarations
//-----------------------------------------------------------------------------

// memory helpers
#define PL_ALLOC(x) gptMemory->tracked_realloc(NULL, x, __FILE__, __LINE__)
#define PL_FREE(x)  gptMemory->tracked_realloc(x, 0, __FILE__, __LINE__)

// network api: general
plNetworkResult pl_initialize_network(void);
void            pl_shutdown_network(void);
plNetworkResult pl_create_address      (const char* pcAddress, const char* pcService, plNetworkAddressFlags, plNetworkAddress** pptAddressOut);
void            pl_destroy_address     (plNetworkAddress**);
void            pl_create_socket       (plSocketFlags, plSocket** pptSocketOut);
void            pl_destroy_socket      (plSocket**);
plNetworkResult pl_bind_socket         (plSocket*, plNetworkAddress*);
plNetworkResult pl_select_sockets      (plSocket**, bool* abSelectedSockets, uint32_t uSocketCount, uint32_t uTimeOutMilliSec);
plNetworkResult pl_send_socket_data_to (plSocket*, plNetworkAddress*, const void* pData, size_t, size_t* pszSentSizeOut);
plNetworkResult pl_get_socket_data_from(plSocket*, void* pOutData, size_t, size_t* pszRecievedSize, plSocketReceiverInfo*);
plNetworkResult pl_connect_socket      (plSocket*, plNetworkAddress* ptAddress);
plNetworkResult pl_send_socket_data    (plSocket*, void* pData, size_t, size_t* pszSentSizeOut);
plNetworkResult pl_get_socket_data     (plSocket*, void* pData, size_t, size_t* pszRecievedSize);
plNetworkResult pl_accept_socket       (plSocket*, plSocket** pptSocketOut);
plNetworkResult pl_listen_socket       (plSocket*);


// thread api: misc
void     pl_sleep(uint32_t millisec);
uint32_t pl_get_hardware_thread_count(void);

// thread api: thread
plThreadResult pl_create_thread (plThreadProcedure, void* pData, plThread** ppThreadOut);
void           pl_destroy_thread(plThread**);
void           pl_join_thread   (plThread*);
void           pl_yield_thread  (void);
uint32_t       pl_get_thread_id (plThread*);

// thread api: mutex
plThreadResult pl_create_mutex (plMutex** ppMutexOut);
void           pl_lock_mutex   (plMutex*);
void           pl_unlock_mutex (plMutex*);
void           pl_destroy_mutex(plMutex**);

// thread api: critical section
plThreadResult pl_create_critical_section (plCriticalSection** pptCriticalSectionOut);
void           pl_destroy_critical_section(plCriticalSection**);
void           pl_enter_critical_section  (plCriticalSection*);
void           pl_leave_critical_section  (plCriticalSection*);

// thread api: semaphore
plThreadResult pl_create_semaphore     (uint32_t uIntialCount, plSemaphore** pptSemaphoreOut);
void           pl_wait_on_semaphore    (plSemaphore*);
bool           pl_try_wait_on_semaphore(plSemaphore*);
void           pl_release_semaphore    (plSemaphore*);
void           pl_destroy_semaphore    (plSemaphore**);

// thread api: thread local storage
plThreadResult pl_allocate_thread_local_key (plThreadKey** pptKeyOut);
void           pl_free_thread_local_key     (plThreadKey**);
void*          pl_allocate_thread_local_data(plThreadKey*, size_t szSize);
void*          pl_get_thread_local_data     (plThreadKey*);
void           pl_free_thread_local_data    (plThreadKey*, void* pData);

// thread api: conditional variable
plThreadResult pl_create_condition_variable  (plConditionVariable** pptConditionVariableOut);
void           pl_destroy_condition_variable (plConditionVariable**);
void           pl_wake_condition_variable    (plConditionVariable*);
void           pl_wake_all_condition_variable(plConditionVariable*);
void           pl_sleep_condition_variable   (plConditionVariable*, plCriticalSection*);

// thread api: barrier
plThreadResult pl_create_barrier (uint32_t uThreadCount, plBarrier** pptBarrierOut);
void           pl_destroy_barrier(plBarrier**);
void           pl_wait_on_barrier(plBarrier*);

// atomics
plThreadResult pl_create_atomic_counter  (int64_t ilValue, plAtomicCounter** ptCounter);
void           pl_destroy_atomic_counter (plAtomicCounter**);
void           pl_atomic_store           (plAtomicCounter*, int64_t ilValue);
int64_t        pl_atomic_load            (plAtomicCounter*);
bool           pl_atomic_compare_exchange(plAtomicCounter*, int64_t ilExpectedValue, int64_t ilDesiredValue);
int64_t        pl_atomic_increment       (plAtomicCounter*);
int64_t        pl_atomic_decrement       (plAtomicCounter*);

// virtual memory
size_t pl_get_page_size   (void);
void*  pl_virtual_alloc   (void* pAddress, size_t);
void*  pl_virtual_reserve (void* pAddress, size_t);
void*  pl_virtual_commit  (void* pAddress, size_t); 
void   pl_virtual_uncommit(void* pAddress, size_t);
void   pl_virtual_free    (void* pAddress, size_t);

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    const plNetworkI tNetworkApi = {
        .initialize           = pl_initialize_network,
        .shutdown             = pl_shutdown_network,
        .create_address       = pl_create_address,
        .destroy_address      = pl_destroy_address,
        .create_socket        = pl_create_socket,
        .destroy_socket       = pl_destroy_socket,
        .bind_socket          = pl_bind_socket,
        .send_socket_data_to  = pl_send_socket_data_to,
        .get_socket_data_from = pl_get_socket_data_from,
        .connect_socket       = pl_connect_socket,
        .get_socket_data      = pl_get_socket_data,
        .listen_socket        = pl_listen_socket,
        .select_sockets       = pl_select_sockets,
        .accept_socket        = pl_accept_socket,
        .send_socket_data     = pl_send_socket_data,
    };

    const plThreadsI tThreadApi = {
        .get_hardware_thread_count   = pl_get_hardware_thread_count,
        .create_thread               = pl_create_thread,
        .destroy_thread              = pl_destroy_thread,
        .join_thread                 = pl_join_thread,
        .yield_thread                = pl_yield_thread,
        .sleep_thread                = pl_sleep,
        .get_thread_id               = pl_get_thread_id,
        .create_mutex                = pl_create_mutex,
        .destroy_mutex               = pl_destroy_mutex,
        .lock_mutex                  = pl_lock_mutex,
        .unlock_mutex                = pl_unlock_mutex,
        .create_semaphore            = pl_create_semaphore,
        .destroy_semaphore           = pl_destroy_semaphore,
        .wait_on_semaphore           = pl_wait_on_semaphore,
        .try_wait_on_semaphore       = pl_try_wait_on_semaphore,
        .release_semaphore           = pl_release_semaphore,
        .allocate_thread_local_key   = pl_allocate_thread_local_key,
        .allocate_thread_local_data  = pl_allocate_thread_local_data,
        .free_thread_local_key       = pl_free_thread_local_key, 
        .get_thread_local_data       = pl_get_thread_local_data, 
        .free_thread_local_data      = pl_free_thread_local_data, 
        .create_critical_section     = pl_create_critical_section,
        .destroy_critical_section    = pl_destroy_critical_section,
        .enter_critical_section      = pl_enter_critical_section,
        .leave_critical_section      = pl_leave_critical_section,
        .create_condition_variable   = pl_create_condition_variable,
        .destroy_condition_variable  = pl_destroy_condition_variable,
        .wake_condition_variable     = pl_wake_condition_variable,
        .wake_all_condition_variable = pl_wake_all_condition_variable,
        .sleep_condition_variable    = pl_sleep_condition_variable,
        .create_barrier              = pl_create_barrier,
        .destroy_barrier             = pl_destroy_barrier,
        .wait_on_barrier             = pl_wait_on_barrier
    };

    const plAtomicsI tAtomicsApi = {
        .create_atomic_counter   = pl_create_atomic_counter,
        .destroy_atomic_counter  = pl_destroy_atomic_counter,
        .atomic_store            = pl_atomic_store,
        .atomic_load             = pl_atomic_load,
        .atomic_compare_exchange = pl_atomic_compare_exchange,
        .atomic_increment        = pl_atomic_increment,
        .atomic_decrement        = pl_atomic_decrement
    };

    const plVirtualMemoryI tVirtualMemoryApi = {
            .get_page_size = pl_get_page_size,
            .alloc         = pl_virtual_alloc,
            .reserve       = pl_virtual_reserve,
            .commit        = pl_virtual_commit,
            .uncommit      = pl_virtual_uncommit,
            .free          = pl_virtual_free,
        };

    pl_set_api(ptApiRegistry, plNetworkI, &tNetworkApi);
    pl_set_api(ptApiRegistry, plThreadsI, &tThreadApi);
    pl_set_api(ptApiRegistry, plAtomicsI, &tAtomicsApi);
    pl_set_api(ptApiRegistry, plVirtualMemoryI, &tVirtualMemoryApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
}

#endif // PL_OS_EXT_INTERNAL_H