/*
   pl_job_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] free list & helper functions
// [SECTION] implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_job_ext.h"
#include <math.h>
#include <string.h> // memset

// extensions
#include "pl_platform_ext.h" // atomics & threads

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plAtomicsI* gptAtomics = NULL;
    static const plThreadsI* gptThreads = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_JOB_THREADS
    #define PL_MAX_JOB_THREADS 64
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plAtomicCounterNode
{
    plAtomicCounter* ptCounter;
    uint32_t         uNextNode;
} plAtomicCounterNode;

typedef struct _plSubmittedBatch
{
    void            (*task)(plInvocationData, void*, void*);
    void*            pData;
    plAtomicCounter* ptCounter;
    plInvocationData tInvocationData;
} plSubmittedBatch;

typedef struct _plJobContext
{
    bool         bRunning;
    uint32_t     uThreadCount;
    plThread*    aptThreads[PL_MAX_JOB_THREADS];
    plThreadKey* ptThreadLocalKey;
    size_t       szSharedMemorySize;
    void*        pThreadLocalData;

    // counter free list data
    plAtomicCounterNode* sbtNodes;
    uint32_t             uFreeList;

    // queue data
    plConditionVariable* ptConditionVariable;
    plCriticalSection*   ptCriticalSection;
    uint32_t             uFrontIndex;
    uint32_t             uBackIndex;
    uint32_t             uBatchCount;
    uint32_t             uBatchCapacity;
    plSubmittedBatch*    sbtBatches; // ring buffer
} plJobContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plJobContext* gptJobCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] free list & helper functions
//-----------------------------------------------------------------------------

static void
pl__job_add_node_to_freelist(uint32_t uNode)
{
    gptJobCtx->sbtNodes[uNode].uNextNode = gptJobCtx->uFreeList;
    gptJobCtx->uFreeList = uNode;
}

static uint32_t
pl__job_acquire_counter_node(void)
{
    // Allocate another node if no free nodes remain.
    if(gptJobCtx->uFreeList == UINT32_MAX)
    {
        plAtomicCounterNode tNewNode = {
            .uNextNode = UINT32_MAX
        };

        gptAtomics->create_counter(0, &tNewNode.ptCounter);
        const uint32_t uNode = (uint32_t)pl_sb_size(gptJobCtx->sbtNodes);
        pl_sb_push(gptJobCtx->sbtNodes, tNewNode);
        return uNode;
    }

    // Pop head of free list.
    const uint32_t uNode = gptJobCtx->uFreeList;
    gptJobCtx->uFreeList = gptJobCtx->sbtNodes[uNode].uNextNode;
    gptJobCtx->sbtNodes[uNode].uNextNode = UINT32_MAX;

    return uNode;
}

// Caller must hold ptCriticalSection
static bool
pl__job_pop_batch_locked(plSubmittedBatch* ptBatchOut)
{
    if(gptJobCtx->uBatchCount == 0)
        return false;

    *ptBatchOut = gptJobCtx->sbtBatches[gptJobCtx->uFrontIndex];

    // Advance front.
    if(gptJobCtx->uFrontIndex == 0)
        gptJobCtx->uFrontIndex = gptJobCtx->uBatchCapacity - 1;
    else
        gptJobCtx->uFrontIndex--;

    gptJobCtx->uBatchCount--;
    return true;
}

static bool
pl__pop_batch_off_queue(plSubmittedBatch* ptBatchOut)
{
    gptThreads->enter_critical_section(gptJobCtx->ptCriticalSection);
    bool bResult = pl__job_pop_batch_locked(ptBatchOut);
    gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);
    return bResult;
}

static void
pl__maybe_grow_batch_capacity(uint32_t uJobCount)
{
    const uint32_t uOldCount = gptJobCtx->uBatchCount;
    const uint32_t uRequired = uOldCount + uJobCount;

    PL_ASSERT(uRequired >= uOldCount); // overflow

    if(uRequired <= gptJobCtx->uBatchCapacity)
    {
        gptJobCtx->uBatchCount = uRequired;
        return;
    }

    const uint32_t uOldCapacity = gptJobCtx->uBatchCapacity;

    // Grow geometrically to avoid frequent allocations.
    uint32_t uNewCapacity = uOldCapacity;

    while(uNewCapacity < uRequired)
    {
        PL_ASSERT(uNewCapacity <= UINT32_MAX / 2);
        uNewCapacity *= 2;
    }

    // Copy existing batches in their execution order.
    plSubmittedBatch* ptPending = NULL;

    if(uOldCount > 0)
    {
        ptPending = PL_ALLOC(sizeof(plSubmittedBatch) * uOldCount);

        uint32_t uIndex = gptJobCtx->uFrontIndex;

        for(uint32_t i = 0; i < uOldCount; i++)
        {
            ptPending[i] = gptJobCtx->sbtBatches[uIndex];

            if(uIndex == 0)
                uIndex = uOldCapacity - 1;
            else
                uIndex--;
        }
    }

    // Resize backing storage.
    pl_sb_resize(gptJobCtx->sbtBatches, uNewCapacity);

    // Place old batches into their new ring positions.
    // Oldest is at uOldCount, newest is at 1.
    for(uint32_t i = 0; i < uOldCount; i++)
    {
        gptJobCtx->sbtBatches[uOldCount - i] = ptPending[i];
    }

    PL_FREE(ptPending);

    // Initialize shared-memory metadata for new slots.
    for(uint32_t i = uOldCapacity; i < uNewCapacity; i++)
    {
        gptJobCtx->sbtBatches[i].tInvocationData.szSharedMemorySize = gptJobCtx->szSharedMemorySize;
    }

    // Restore ring invariants.
    gptJobCtx->uFrontIndex = uOldCount;
    gptJobCtx->uBackIndex = 0;
    gptJobCtx->uBatchCapacity = uNewCapacity;
    gptJobCtx->uBatchCount = uRequired;
}

static void*
pl__thread_procedure(void* pData)
{

    // allocate thread local storage data for groups
    void* pThreadLocalData = NULL;
    if(gptJobCtx->szSharedMemorySize > 0)
    {
        pThreadLocalData = gptThreads->allocate_thread_local_data(gptJobCtx->ptThreadLocalKey, gptJobCtx->szSharedMemorySize);
        memset(pThreadLocalData, 0, gptJobCtx->szSharedMemorySize);
    }

    gptThreads->enter_critical_section(gptJobCtx->ptCriticalSection);

    // Wait while there is no work and the system is running.
    while(gptJobCtx->bRunning && gptJobCtx->uBatchCount == 0)
    {
        gptThreads->sleep_condition_variable(
            gptJobCtx->ptConditionVariable,
            gptJobCtx->ptCriticalSection);
    }

    // Shutdown after the queue has been drained.
    if(!gptJobCtx->bRunning && gptJobCtx->uBatchCount == 0)
    {
        gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);
        return NULL;
    }

    // Guaranteed to have work here.
    plSubmittedBatch tBatch = {0};
    pl__job_pop_batch_locked(&tBatch);

    gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);

    // Execute OUTSIDE the lock.
    plInvocationData tInvocationData = tBatch.tInvocationData;

    for(uint32_t i = 0; i < tBatch.tInvocationData.uBatchSize; i++)
    {
        tInvocationData.uLocalIndex = i;
        tInvocationData.uGlobalIndex =
            tBatch.tInvocationData.uGlobalIndex + i;

        tBatch.task(tInvocationData, tBatch.pData, pThreadLocalData);
    }

    if(tBatch.ptCounter)
        gptAtomics->decrement(tBatch.ptCounter);

    if(gptJobCtx->szSharedMemorySize > 0)
        gptThreads->free_thread_local_data(gptJobCtx->ptThreadLocalKey, pThreadLocalData);
    return NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

void
pl_job_dispatch_jobs(uint32_t uJobCount, plJobDesc* ptJobs, plAtomicCounter** pptCounter)
{

    if(pptCounter)
        *pptCounter = NULL;

    if(uJobCount == 0)
        return;

    plAtomicCounter* ptCounter = NULL;

    gptThreads->enter_critical_section(gptJobCtx->ptCriticalSection);

    if(pptCounter)
    {
        // get free atomic counter node
        uint32_t uNode = pl__job_acquire_counter_node();
        
        ptCounter = gptJobCtx->sbtNodes[uNode].ptCounter;
        *pptCounter = ptCounter;

        // store job count into counter
        gptAtomics->store(ptCounter, (uint64_t)uJobCount);
    }

    // set total job count in queue
    pl__maybe_grow_batch_capacity(uJobCount);

    // push jobs into queue
    for(uint32_t i = 0; i < uJobCount; i++)
    {
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].task = ptJobs[i].task;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].pData = ptJobs[i].pData;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].ptCounter = ptCounter;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchIndex = 0;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uGlobalIndex = i;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchSize = 1;
        gptJobCtx->uBackIndex--;
        if(gptJobCtx->uBackIndex == UINT32_MAX) // wrap around
            gptJobCtx->uBackIndex = gptJobCtx->uBatchCapacity - 1;
    }

    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
    gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);

    // wake any sleeping threads
    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
}

void
pl_job_dispatch_batch(uint32_t uJobCount, uint32_t uGroupSize, plJobDesc tJobDesc, plAtomicCounter** pptCounter)
{

    if(pptCounter)
        *pptCounter = NULL;

    if(uJobCount == 0)
        return;

    plAtomicCounter* ptCounter = NULL;

    // find optimal group size
    if(uGroupSize == 0)
    {
        uGroupSize = (uint32_t)floorf((float)uJobCount / (float)gptJobCtx->uThreadCount);

        // possible if job count is less than thread count
        if(uGroupSize == 0)
            uGroupSize = 1;
    }

    if(uGroupSize > uJobCount)
        uGroupSize = uJobCount;

    gptThreads->enter_critical_section(gptJobCtx->ptCriticalSection);
        
    uint32_t uBatches = (uint32_t)floorf((float)uJobCount / (float)uGroupSize);
    uint32_t uLeftOverJobs = uJobCount % uGroupSize;

    if(pptCounter)
    {
        uint32_t uNode = pl__job_acquire_counter_node();
        ptCounter = gptJobCtx->sbtNodes[uNode].ptCounter;
        *pptCounter = ptCounter;
        gptAtomics->store(ptCounter, (uint64_t)uBatches + (uLeftOverJobs > 0 ? 1 : 0));
    }

    pl__maybe_grow_batch_capacity(uBatches + (uLeftOverJobs > 0 ? 1 : 0));

    // push batches into queue
    for(uint32_t i = 0; i < uBatches; i++)
    {
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].task = tJobDesc.task;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].pData = tJobDesc.pData;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].ptCounter = ptCounter;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uGlobalIndex = i * uGroupSize;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchSize = uGroupSize;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchIndex = i;
        gptJobCtx->uBackIndex--;
        if(gptJobCtx->uBackIndex == UINT32_MAX) // wrap around
            gptJobCtx->uBackIndex = gptJobCtx->uBatchCapacity - 1;
    }

    if(uLeftOverJobs > 0)
    {
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].task = tJobDesc.task;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].pData = tJobDesc.pData;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].ptCounter = ptCounter;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uGlobalIndex = uBatches * uGroupSize;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchIndex = uBatches;
        gptJobCtx->sbtBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchSize = uLeftOverJobs;
        gptJobCtx->uBackIndex--;
        if(gptJobCtx->uBackIndex == UINT32_MAX) // wrap around
            gptJobCtx->uBackIndex = gptJobCtx->uBatchCapacity - 1; 
    }

    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
    gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);

    // wake any sleeping threads
    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
}

void
pl_job_wait_for_counter(plAtomicCounter* ptCounter)
{
    if(ptCounter == NULL)
        return;
        
    const uint32_t uValue = 0;

    // wait for counter to reach value (or less)
    while(true)
    {
        const int64_t ilLoadedValue = gptAtomics->load(ptCounter);
        if(ilLoadedValue <= (int64_t)uValue)
            break;

        gptThreads->wake_condition_variable(gptJobCtx->ptConditionVariable);

        // check for available batch (instead of wasting main thread)
        plSubmittedBatch tBatch = {0};
        if(pl__pop_batch_off_queue(&tBatch))
        {
            // run tasks
            plInvocationData tInvocationData = tBatch.tInvocationData;
            for(uint32_t i = 0; i < tBatch.tInvocationData.uBatchSize; i++)
            {
                // set per job invocation members
                tInvocationData.uLocalIndex = i;
                tInvocationData.uGlobalIndex = tBatch.tInvocationData.uGlobalIndex + i;

                // run actual job
                tBatch.task(tInvocationData, tBatch.pData, gptJobCtx->pThreadLocalData);
            }

            // decrement atomic counter
            if(tBatch.ptCounter)
                gptAtomics->decrement(tBatch.ptCounter);
        }
    }

    gptThreads->enter_critical_section(gptJobCtx->ptCriticalSection);

    // find counter index & return to free list
    bool bFound = false;
    const uint32_t uNodeCount = pl_sb_size(gptJobCtx->sbtNodes);
    for(uint32_t i = 0; i < uNodeCount; i++)
    {
        if(gptJobCtx->sbtNodes[i].ptCounter == ptCounter)
        {
            pl__job_add_node_to_freelist(i);
            bFound = true;
            break;
        }
    }
    PL_ASSERT(bFound);
    
    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
    gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);
}

bool
pl_job_is_shutting_down(void)
{
    return !gptJobCtx->bRunning;
}

uint32_t
pl_job_get_thread_count(void)
{
    return gptJobCtx->uThreadCount;
}

void
pl_job_initialize(plJobSystemInit tInit)
{

    // already initialized
    if(gptJobCtx->uThreadCount > 0)
        return;

    // allocate & store context
    const uint32_t uHardwareThreadCount = gptThreads->get_hardware_thread_count();

    if(tInit.uThreadCount == 0)
        tInit.uThreadCount = uHardwareThreadCount - 1;

    if(tInit.uThreadCount > uHardwareThreadCount)
        tInit.uThreadCount = uHardwareThreadCount - 1;

    gptJobCtx->szSharedMemorySize = tInit.szSharedMemorySize;

    PL_ASSERT(tInit.uThreadCount < PL_MAX_JOB_THREADS);
    gptJobCtx->bRunning = true;
    gptJobCtx->uThreadCount = tInit.uThreadCount;
    gptThreads->create_condition_variable(&gptJobCtx->ptConditionVariable);
    gptThreads->create_critical_section(&gptJobCtx->ptCriticalSection);

    gptJobCtx->uBatchCapacity = 128;
    pl_sb_resize(gptJobCtx->sbtNodes, gptJobCtx->uBatchCapacity);
    pl_sb_resize(gptJobCtx->sbtBatches, gptJobCtx->uBatchCapacity);
    for(uint32_t i = 0; i < gptJobCtx->uBatchCapacity; i++)
    {
        gptJobCtx->sbtBatches[i].task = NULL;
        gptJobCtx->sbtBatches[i].pData = NULL;
        gptJobCtx->sbtBatches[i].tInvocationData.uBatchIndex = UINT32_MAX;
        gptJobCtx->sbtBatches[i].tInvocationData.uGlobalIndex = UINT32_MAX;
        gptJobCtx->sbtBatches[i].tInvocationData.uBatchSize = UINT32_MAX;
        gptJobCtx->sbtBatches[i].tInvocationData.szSharedMemorySize = tInit.szSharedMemorySize;
        gptAtomics->create_counter(0, &gptJobCtx->sbtNodes[i].ptCounter);
        gptJobCtx->sbtNodes[i].uNextNode = i + 1;
    }
    gptJobCtx->sbtNodes[gptJobCtx->uBatchCapacity - 1].uNextNode = UINT32_MAX;

    gptJobCtx->uFreeList = 0;

    // allocate thread local key if needed
    if(gptJobCtx->szSharedMemorySize > 0)
        gptThreads->allocate_thread_local_key(&gptJobCtx->ptThreadLocalKey);

    if(gptJobCtx->szSharedMemorySize > 0)
    {
        gptJobCtx->pThreadLocalData = gptThreads->allocate_thread_local_data(gptJobCtx->ptThreadLocalKey, gptJobCtx->szSharedMemorySize);
        memset(gptJobCtx->pThreadLocalData, 0, gptJobCtx->szSharedMemorySize);
    }

    for(uint32_t i = 0; i < tInit.uThreadCount; i++)
    {
        gptThreads->create_thread(pl__thread_procedure, NULL, &gptJobCtx->aptThreads[i]);
    }
}

void
pl_job_cleanup(void)
{
    gptThreads->enter_critical_section(gptJobCtx->ptCriticalSection);
    gptJobCtx->bRunning = false;
    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
    gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);
    for(uint32_t i = 0; i < gptJobCtx->uThreadCount; i++)
    {
        gptThreads->destroy_thread(&gptJobCtx->aptThreads[i]);
    }

    if(gptJobCtx->szSharedMemorySize > 0)
        gptThreads->free_thread_local_data(gptJobCtx->ptThreadLocalKey, gptJobCtx->pThreadLocalData);

    // free thread local key if needed
    if(gptJobCtx->szSharedMemorySize > 0)
        gptThreads->free_thread_local_key(&gptJobCtx->ptThreadLocalKey);

    gptThreads->destroy_condition_variable(&gptJobCtx->ptConditionVariable);
    gptThreads->destroy_critical_section(&gptJobCtx->ptCriticalSection);

    for(uint32_t i = 0; i < gptJobCtx->uBatchCapacity; i++)
    {
        gptJobCtx->sbtBatches[i].task = NULL;
        gptJobCtx->sbtBatches[i].pData = NULL;
        gptJobCtx->sbtBatches[i].tInvocationData.uGlobalIndex = UINT32_MAX;
    }

    for(uint32_t i = 0; i < pl_sb_size(gptJobCtx->sbtNodes); i++)
    {
        gptAtomics->destroy_counter(&gptJobCtx->sbtNodes[i].ptCounter);
    }

    pl_sb_free(gptJobCtx->sbtBatches);
    pl_sb_free(gptJobCtx->sbtNodes);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_job_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plJobI tApi = {
        .initialize       = pl_job_initialize,
        .cleanup          = pl_job_cleanup,
        .wait_for_counter = pl_job_wait_for_counter,
        .dispatch_jobs    = pl_job_dispatch_jobs,
        .dispatch_batch   = pl_job_dispatch_batch,
        .is_shutting_down = pl_job_is_shutting_down,
        .get_thread_count = pl_job_get_thread_count,
    };
    pl_set_api(ptApiRegistry, plJobI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptAtomics = pl_get_api_latest(ptApiRegistry, plAtomicsI);
        gptThreads = pl_get_api_latest(ptApiRegistry, plThreadsI);
    #endif
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptJobCtx = ptDataRegistry->get_data("plJobContext");
    }
    else
    {
        static plJobContext gtJobCtx = {0};
        gptJobCtx = &gtJobCtx;
        ptDataRegistry->set_data("plJobContext", gptJobCtx);
    }
}

void
pl_unload_job_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plJobI* ptApi = pl_get_api_latest(ptApiRegistry, plJobI);
    ptApiRegistry->remove_api(ptApi);
}