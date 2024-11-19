/*
   pl_thread_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_THREADS_EXT_H
#define PL_THREADS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plThreadsI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plThread            plThread;            // opaque type (used by platform backends)
typedef struct _plMutex             plMutex;             // opaque type (used by platform backends)
typedef struct _plCriticalSection   plCriticalSection;   // opaque type (used by platform backends)
typedef struct _plSemaphore         plSemaphore;         // opaque type (used by platform backends)
typedef struct _plBarrier           plBarrier;           // opaque type (used by platform backends)
typedef struct _plConditionVariable plConditionVariable; // opaque type (used by platform backends)
typedef struct _plThreadKey         plThreadKey;         // opaque type (used by platform backends)

// enums
typedef int plThreadResult; // -> enum _plThreadResult // Enum:

// thread procedure signature
typedef void* (*plThreadProcedure)(void*);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plThreadResult
{
    PL_THREAD_RESULT_FAIL    = 0,
    PL_THREAD_RESULT_SUCCESS = 1
};

#endif // PL_THREADS_EXT_H