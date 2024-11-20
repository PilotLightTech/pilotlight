/*
   pl_job_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] free list functions
// [SECTION] implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_job_ext.h"
#include "pl_threads_ext.h"
#include "pl_atomics_ext.h"
#include <math.h>
#include <string.h>

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    static const plAtomicsI* gptAtomics = NULL;
    static const plThreadsI* gptThreads = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_BATCHES
    #define PL_MAX_BATCHES 64
#endif

#ifndef PL_MAX_JOB_THREADS
    #define PL_MAX_JOB_THREADS 64
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plAtomicCounterNode
{
    plAtomicCounter* ptCounter;
    uint32_t         uNodeIndex;
    uint32_t         uNextNode;
} plAtomicCounterNode;

typedef struct _plSubmittedBatch
{
    void            (*task)(plInvocationData, void*);
    void*            pData;
    plAtomicCounter* ptCounter;
    plInvocationData tInvocationData;
} plSubmittedBatch;

typedef struct _plJobContext
{
    bool      bRunning;
    uint32_t  uThreadCount;
    plThread* aptThreads[PL_MAX_JOB_THREADS];

    // counter free list data
    plAtomicCounterNode atNodes[PL_MAX_BATCHES];
    uint32_t            uFreeList;

    // queue data
    plConditionVariable* ptConditionVariable;
    plCriticalSection*   ptCriticalSection;
    plAtomicCounter*     ptQueueLatch; // 1 - locked, 0 - unlocked
    uint32_t             uFrontIndex;
    uint32_t             uBackIndex;
    uint32_t             uBatchCount;
    plSubmittedBatch     atBatches[PL_MAX_BATCHES]; // ring buffer
} plJobContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plJobContext* gptJobCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] free list functions
//-----------------------------------------------------------------------------

static void
pl__job_add_node_to_freelist(uint32_t uNode)
{
    gptJobCtx->atNodes[uNode].uNextNode = gptJobCtx->uFreeList;
    gptJobCtx->uFreeList = uNode;
}

static void
pl__job_remove_node_from_freelist(uint32_t uNode)
{

    bool bFound = false;
    if(gptJobCtx->uFreeList == uNode)
    {
        gptJobCtx->uFreeList = gptJobCtx->atNodes[uNode].uNextNode;
        bFound = true;
    }
    else
    {
        uint32_t uNextNode = gptJobCtx->uFreeList;
        while(uNextNode != UINT32_MAX)
        {
            uint32_t uPrevNode = uNextNode;
            uNextNode = gptJobCtx->atNodes[uPrevNode].uNextNode;
            
            if(uNextNode == uNode)
            {
                gptJobCtx->atNodes[uPrevNode].uNextNode = gptJobCtx->atNodes[uNode].uNextNode;
                bFound = true;
                break;
            }
        }
    }

    plAtomicCounterNode* ptNode = &gptJobCtx->atNodes[uNode];
    ptNode->uNextNode = UINT32_MAX;
    PL_ASSERT(bFound && "could not find node to remove");
}

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl__dispatch_jobs(uint32_t uJobCount, plJobDesc* ptJobs, plAtomicCounter** pptCounter)
{

    plAtomicCounter* ptCounter = NULL;

    // try to unlock (spin lock)
    while(true)
    {
        if(gptAtomics->atomic_compare_exchange(gptJobCtx->ptQueueLatch, 0, 1))
        {

            if(pptCounter)
            {
                // get free atomic counter node
                uint32_t uNode = gptJobCtx->uFreeList;
                pl__job_remove_node_from_freelist(uNode);
                
                ptCounter = gptJobCtx->atNodes[uNode].ptCounter;
                *pptCounter = ptCounter;

                // store job count into counter
                gptAtomics->atomic_store(ptCounter, (uint64_t)uJobCount);
            }

            // set total job count in queue
            gptJobCtx->uBatchCount += uJobCount;
            PL_ASSERT(gptJobCtx->uBatchCount < PL_MAX_BATCHES);

            // push jobs into queue
            for(uint32_t i = 0; i < uJobCount; i++)
            {
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].task = ptJobs[i].task;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].pData = ptJobs[i].pData;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].ptCounter = ptCounter;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchIndex = 0;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uGlobalIndex = i;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchSize = 1;
                gptJobCtx->uBackIndex--;
                if(gptJobCtx->uBackIndex == UINT32_MAX) // wrap around
                    gptJobCtx->uBackIndex = PL_MAX_BATCHES - 1;
            }
            break;
        }
    }

    // unlock
    gptAtomics->atomic_store(gptJobCtx->ptQueueLatch, 0);

    // wake any sleeping threads
    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
}

static bool
pl__pop_batch_off_queue(plSubmittedBatch* ptBatchOut)
{
    bool bHasBatch = false;

    // try to unlock (spin lock)
    while(true)
    {
        if(gptAtomics->atomic_compare_exchange(gptJobCtx->ptQueueLatch, 0, 1))
        {
            if(gptJobCtx->uBatchCount != 0)
            {
                *ptBatchOut = gptJobCtx->atBatches[gptJobCtx->uFrontIndex];
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].pData = NULL;
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].task = NULL;
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].ptCounter = NULL;
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].tInvocationData.uBatchSize = UINT32_MAX;
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].tInvocationData.uGlobalIndex = UINT32_MAX;
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].tInvocationData.uBatchIndex = UINT32_MAX;
                gptJobCtx->uFrontIndex--;
                if(gptJobCtx->uFrontIndex == UINT32_MAX) // wrap
                    gptJobCtx->uFrontIndex = PL_MAX_BATCHES - 1;

                // update total job count
                gptJobCtx->uBatchCount--;
                bHasBatch = true;
            }
            break;
        }
    }

    // unlock
    gptAtomics->atomic_store(gptJobCtx->ptQueueLatch, 0);

    return bHasBatch;
}

static void
pl__dispatch_batch(uint32_t uJobCount, uint32_t uGroupSize, plJobDesc tJobDesc, plAtomicCounter** pptCounter)
{

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

    // try to unlock (spin lock)
    while(true)
    {
        if(gptAtomics->atomic_compare_exchange(gptJobCtx->ptQueueLatch, 0, 1))
        {
            
            uint32_t uBatches = (uint32_t)floorf((float)uJobCount / (float)uGroupSize);
            uint32_t uLeftOverJobs = uJobCount % uGroupSize;

            if(pptCounter)
            {
                uint32_t uNode = gptJobCtx->uFreeList;
                pl__job_remove_node_from_freelist(uNode);
                ptCounter = gptJobCtx->atNodes[uNode].ptCounter;
                *pptCounter = ptCounter;
                gptAtomics->atomic_store(ptCounter, (uint64_t)uBatches + (uLeftOverJobs > 0 ? 1 : 0));
            }

            gptJobCtx->uBatchCount += uBatches;
            PL_ASSERT(gptJobCtx->uBatchCount < PL_MAX_BATCHES);

            // push batches into queue
            for(uint32_t i = 0; i < uBatches; i++)
            {
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].task = tJobDesc.task;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].pData = tJobDesc.pData;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].ptCounter = ptCounter;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uGlobalIndex = i * uGroupSize;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchSize = uGroupSize;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchIndex = i;
                gptJobCtx->uBackIndex--;
                if(gptJobCtx->uBackIndex == UINT32_MAX) // wrap around
                    gptJobCtx->uBackIndex = PL_MAX_BATCHES - 1;
            }

            if(uLeftOverJobs > 0)
            {
                gptJobCtx->uBatchCount++;
                PL_ASSERT(gptJobCtx->uBatchCount < PL_MAX_BATCHES);
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].task = tJobDesc.task;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].pData = tJobDesc.pData;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].ptCounter = ptCounter;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uGlobalIndex = uBatches * uGroupSize;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchIndex = uBatches;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].tInvocationData.uBatchSize = uLeftOverJobs;
                gptJobCtx->uBackIndex--;
                if(gptJobCtx->uBackIndex == UINT32_MAX) // wrap around
                    gptJobCtx->uBackIndex = PL_MAX_BATCHES - 1; 
            }

            break;
        }
    }

    // unlock
    gptAtomics->atomic_store(gptJobCtx->ptQueueLatch, 0);

    // wake any sleeping threads
    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
}

static void
pl__wait_for_counter(plAtomicCounter* ptCounter)
{
    if(ptCounter == NULL)
        return;
        
    const uint32_t uValue = 0;

    // wait for counter to reach value (or less)
    while(true)
    {
        const int64_t ilLoadedValue = gptAtomics->atomic_load(ptCounter);
        if(ilLoadedValue <= (int64_t)uValue)
            break;
        gptThreads->wake_condition_variable(gptJobCtx->ptConditionVariable);
    }

    // try to unlock (spin lock)
    while(true)
    {
        if(gptAtomics->atomic_compare_exchange(gptJobCtx->ptQueueLatch, 0, 1))
        {
            // find counter index & return to free list
            bool bFound = false;
            for(uint32_t i = 0; i < PL_MAX_BATCHES; i++)
            {
                if(gptJobCtx->atNodes[i].ptCounter == ptCounter)
                {
                    pl__job_add_node_to_freelist(i);
                    bFound = true;
                    break;
                }
            }
            PL_ASSERT(bFound);
            break;
        }
    }

    // unlock
    gptAtomics->atomic_store(gptJobCtx->ptQueueLatch, 0);
}

static void*
pl__thread_procedure(void* pData)
{
    // check for available job
    plSubmittedBatch tBatch = {0};
    while(gptJobCtx->bRunning)
    {
        
        if(pl__pop_batch_off_queue(&tBatch))
        {
            // run tasks
            plInvocationData tInvocationData = tBatch.tInvocationData;
            for(uint32_t i = 0; i < tBatch.tInvocationData.uBatchSize; i++)
            {
                tInvocationData.uLocalIndex = i;
                tInvocationData.uGlobalIndex = tBatch.tInvocationData.uGlobalIndex + i;
                tBatch.task(tInvocationData, tBatch.pData);
            }

            // decrement atomic counter
            if(tBatch.ptCounter)
                gptAtomics->atomic_decrement(tBatch.ptCounter);

        }
        else // no jobs
        {
            // sleep thread based on conditional variable (to be awaken once new jobs are pushed onto queue)
            gptThreads->enter_critical_section(gptJobCtx->ptCriticalSection);
            gptThreads->sleep_condition_variable(gptJobCtx->ptConditionVariable, gptJobCtx->ptCriticalSection);
            gptThreads->leave_critical_section(gptJobCtx->ptCriticalSection);
        }
    }
    return NULL;
}

static void
pl__initialize(uint32_t uThreadCount)
{

    // allocate & store context
    const uint32_t uHardwareThreadCount = gptThreads->get_hardware_thread_count();

    if(uThreadCount == 0)
        uThreadCount = uHardwareThreadCount - 1;

    if(uThreadCount > uHardwareThreadCount)
        uThreadCount = uHardwareThreadCount - 1;

    PL_ASSERT(uThreadCount < PL_MAX_JOB_THREADS);
    gptJobCtx->bRunning = true;
    gptJobCtx->uThreadCount = uThreadCount;
    gptAtomics->create_atomic_counter(0, &gptJobCtx->ptQueueLatch);
    gptThreads->create_condition_variable(&gptJobCtx->ptConditionVariable);
    gptThreads->create_critical_section(&gptJobCtx->ptCriticalSection);

    for(uint32_t i = 0; i < PL_MAX_BATCHES; i++)
    {
        gptJobCtx->atBatches[i].task = NULL;
        gptJobCtx->atBatches[i].pData = NULL;
        gptJobCtx->atBatches[i].tInvocationData.uBatchIndex = UINT32_MAX;
        gptJobCtx->atBatches[i].tInvocationData.uGlobalIndex = UINT32_MAX;
        gptJobCtx->atBatches[i].tInvocationData.uBatchSize = UINT32_MAX;
        gptAtomics->create_atomic_counter(0, &gptJobCtx->atNodes[i].ptCounter);
        gptJobCtx->atNodes[i].uNodeIndex = i;
        gptJobCtx->atNodes[i].uNextNode = i + 1;
    }
    gptJobCtx->uFreeList = 0;

    for(uint32_t i = 0; i < uThreadCount; i++)
        gptThreads->create_thread(pl__thread_procedure, NULL, &gptJobCtx->aptThreads[i]);
}

static void
pl__cleanup(void)
{
    gptJobCtx->bRunning = false;
    gptThreads->wake_all_condition_variable(gptJobCtx->ptConditionVariable);
    for(uint32_t i = 0; i < gptJobCtx->uThreadCount; i++)
        gptThreads->destroy_thread(&gptJobCtx->aptThreads[i]);

    gptAtomics->destroy_atomic_counter(&gptJobCtx->ptQueueLatch);
    gptThreads->destroy_condition_variable(&gptJobCtx->ptConditionVariable);
    gptThreads->destroy_critical_section(&gptJobCtx->ptCriticalSection);

    for(uint32_t i = 0; i < PL_MAX_BATCHES; i++)
    {
        gptJobCtx->atBatches[i].task = NULL;
        gptJobCtx->atBatches[i].pData = NULL;
        gptJobCtx->atBatches[i].tInvocationData.uGlobalIndex = UINT32_MAX;
        gptAtomics->destroy_atomic_counter(&gptJobCtx->atNodes[i].ptCounter);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_job_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plJobI tApi = {
        .initialize       = pl__initialize,
        .cleanup          = pl__cleanup,
        .wait_for_counter = pl__wait_for_counter,
        .dispatch_jobs    = pl__dispatch_jobs,
        .dispatch_batch   = pl__dispatch_batch
    };
    pl_set_api(ptApiRegistry, plJobI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptAtomics = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    gptThreads = pl_get_api_latest(ptApiRegistry, plThreadsI);
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

PL_EXPORT void
pl_unload_job_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plJobI* ptApi = pl_get_api_latest(ptApiRegistry, plJobI);
    ptApiRegistry->remove_api(ptApi);
}