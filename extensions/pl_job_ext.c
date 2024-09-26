/*
   pl_job_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] free list functions
// [SECTION] implementation
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_job_ext.h"
#include "pl_os.h"
#include <math.h>
#include <string.h>
#include "pl_ext.inc"

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
    void            (*task)(uint32_t, void*);
    void*            pData;
    plAtomicCounter* ptCounter;
    uint32_t         uJobIndex;
    uint32_t         uGroupSize;
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
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].uJobIndex = i;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].uGroupSize = 1;
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
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].uGroupSize = UINT32_MAX;
                gptJobCtx->atBatches[gptJobCtx->uFrontIndex].uJobIndex = UINT32_MAX;
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
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].uJobIndex = i * uGroupSize;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].uGroupSize = uGroupSize;
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
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].uJobIndex = uBatches * uGroupSize;
                gptJobCtx->atBatches[gptJobCtx->uBackIndex].uGroupSize = uLeftOverJobs;
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
            for(uint32_t i = 0; i < tBatch.uGroupSize; i++)
                tBatch.task(tBatch.uJobIndex + i, tBatch.pData);

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
        gptJobCtx->atBatches[i].uJobIndex = UINT32_MAX;
        gptJobCtx->atBatches[i].uGroupSize = UINT32_MAX;
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
        gptJobCtx->atBatches[i].uJobIndex = UINT32_MAX;
        gptAtomics->destroy_atomic_counter(&gptJobCtx->atNodes[i].ptCounter);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static const plJobI*
pl_load_job_api(void)
{
    static const plJobI tApi = {
        .initialize       = pl__initialize,
        .cleanup          = pl__cleanup,
        .wait_for_counter = pl__wait_for_counter,
        .dispatch_jobs    = pl__dispatch_jobs,
        .dispatch_batch   = pl__dispatch_batch
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_job_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->add(PL_API_JOB, pl_load_job_api());
    if(bReload)
    {
        gptJobCtx = gptDataRegistry->get_data("plJobContext");
    }
    else
    {
        static plJobContext gtJobCtx = {0};
        gptJobCtx = &gtJobCtx;
        gptDataRegistry->set_data("plJobContext", gptJobCtx);
    }
}

static void
pl_unload_job_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    ptApiRegistry->remove(pl_load_job_api());
}