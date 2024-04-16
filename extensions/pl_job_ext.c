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

#include "pilotlight.h"
#include "pl_job_ext.h"
#include "pl_os.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plAtomicCounterNode
{
    plAtomicCounter* ptCounter;
    uint32_t         uNodeIndex;
    uint32_t         uNextNode;
} plAtomicCounterNode;

typedef struct _plSubmittedJob
{
    plJobDesc        tJob;
    plAtomicCounter* ptCounter;
    uint32_t         uNodeIndex;
} plSubmittedJob;

typedef struct _plJobManagerData
{
    bool      bRunning;
    uint32_t  uThreadCount;
    plThread* aptThreads[PL_MAX_JOB_THREADS];

    // counter free list data
    plAtomicCounterNode atNodes[PL_MAX_JOBS];
    uint32_t            uFreeList;

    // queue data
    plConditionVariable* ptConditionVariable;
    plCriticalSection*   ptCriticalSection;
    plAtomicCounter*     ptQueueLatch; // 1 - locked, 0 - unlocked
    uint32_t             uFrontIndex;
    uint32_t             uBackIndex;
    uint32_t             uJobCount;
    plSubmittedJob       atJobs[PL_MAX_JOBS]; // ring buffer
} plJobManagerData;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static const plThreadsI* gptThreads = NULL;
static const plAtomicsI* gptAtomics = NULL;

static plJobManagerData gptData = {0};

//-----------------------------------------------------------------------------
// [SECTION] free list functions
//-----------------------------------------------------------------------------

static void
pl__add_node_to_freelist(uint32_t uNode)
{
    gptData.atNodes[uNode].uNextNode = gptData.uFreeList;
    gptData.uFreeList = uNode;
}

static void
pl__remove_node_from_freelist(uint32_t uNode)
{

    bool bFound = false;
    if(gptData.uFreeList == uNode)
    {
        gptData.uFreeList = gptData.atNodes[uNode].uNextNode;
        bFound = true;
    }
    else
    {
        uint32_t uNextNode = gptData.uFreeList;
        while(uNextNode != UINT32_MAX)
        {
            uint32_t uPrevNode = uNextNode;
            uNextNode = gptData.atNodes[uPrevNode].uNextNode;
            
            if(uNextNode == uNode)
            {
                gptData.atNodes[uPrevNode].uNextNode = gptData.atNodes[uNode].uNextNode;
                bFound = true;
                break;
            }
        }
    }

    plAtomicCounterNode* ptNode = &gptData.atNodes[uNode];
    ptNode->uNextNode = UINT32_MAX;
    PL_ASSERT(bFound && "could not find node to remove");
}

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl__push_jobs_into_queue(plJobDesc* ptJobs, uint32_t uJobCount, plAtomicCounter** pptCounter)
{

    // try to unlock (spin lock)
    while(true)
    {
        if(gptAtomics->atomic_compare_exchange(gptData.ptQueueLatch, 0, 1))
        {

            // get free atomic counter node
            uint32_t uNode = gptData.uFreeList;
            pl__remove_node_from_freelist(uNode);
            *pptCounter = gptData.atNodes[uNode].ptCounter;

            // store job count into counter
            gptAtomics->atomic_store(*pptCounter, (uint64_t)uJobCount);

            // set total job count in queue
            gptData.uJobCount += uJobCount;
            PL_ASSERT(gptData.uJobCount < PL_MAX_JOBS);

            // push jobs into queue
            for(uint32_t i = 0; i < uJobCount; i++)
            {
                gptData.atJobs[gptData.uBackIndex].tJob = ptJobs[i];
                gptData.atJobs[gptData.uBackIndex].ptCounter = *pptCounter;
                gptData.atJobs[gptData.uBackIndex].uNodeIndex = uNode;
                gptData.uBackIndex--;
                if(gptData.uBackIndex == UINT32_MAX) // wrap around
                    gptData.uBackIndex = PL_MAX_JOBS - 1;
            }
            break;
        }
    }

    // unlock
    gptAtomics->atomic_store(gptData.ptQueueLatch, 0);

    // wake any sleeping threads
    gptThreads->wake_all_condition_variable(gptData.ptConditionVariable);
}

static bool
pl__pop_job_off_queue(plSubmittedJob* ptJobOut)
{
    bool bHasJob = false;

    // try to unlock (spin lock)
    while(true)
    {
        if(gptAtomics->atomic_compare_exchange(gptData.ptQueueLatch, 0, 1))
        {
            if(gptData.uJobCount != 0)
            {
                *ptJobOut = gptData.atJobs[gptData.uFrontIndex--];
                if(gptData.uFrontIndex == UINT32_MAX) // wrap
                    gptData.uFrontIndex = PL_MAX_JOBS - 1;

                // update total job count
                gptData.uJobCount--;
                bHasJob = true;
            }
            break;
        }
    }

    // unlock
    gptAtomics->atomic_store(gptData.ptQueueLatch, 0);

    return bHasJob;
}

static void
pl__wait_for_counter(plAtomicCounter* ptCounter, uint32_t uValue)
{
    // wait for counter to reach value (or less)
    while(true)
    {
        const int64_t ilLoadedValue = gptAtomics->atomic_load(ptCounter);
        if(ilLoadedValue <= (int64_t)uValue)
            break;
    }

    // try to unlock (spin lock)
    while(true)
    {
        if(gptAtomics->atomic_compare_exchange(gptData.ptQueueLatch, 0, 1))
        {
            // find counter index & return to free list
            bool bFound = false;
            for(uint32_t i = 0; i < PL_MAX_JOBS; i++)
            {
                if(gptData.atNodes[i].ptCounter == ptCounter)
                {
                    pl__add_node_to_freelist(i);
                    bFound = true;
                    break;
                }
            }
            PL_ASSERT(bFound);
            break;
        }
    }

    // unlock
    gptAtomics->atomic_store(gptData.ptQueueLatch, 0);
}

static void*
pl__thread_procedure(void* pData)
{
    // check for available job
    plSubmittedJob tJob = {0};
    while(gptData.bRunning)
    {
        
        if(pl__pop_job_off_queue(&tJob))
        {
            // run task
            tJob.tJob.task(tJob.tJob.pData);

            // decrement atomic counter
            gptAtomics->atomic_decrement(tJob.ptCounter);

            // reset job
            tJob.tJob.task = NULL;
            tJob.tJob.pData = NULL;
            tJob.ptCounter = NULL;
        }
        else // no jobs
        {
            // sleep thread based on conditional variable (to be awaken once new jobs are pushed onto queue)
            gptThreads->enter_critical_section(gptData.ptCriticalSection);
            gptThreads->sleep_condition_variable(gptData.ptConditionVariable, gptData.ptCriticalSection);
            gptThreads->leave_critical_section(gptData.ptCriticalSection);
        }
    }
    return NULL;
}

static void
pl__initialize(uint32_t uThreadCount)
{

    if(uThreadCount == 0)
        uThreadCount = gptThreads->get_hardware_thread_count() - 1;

    PL_ASSERT(uThreadCount < PL_MAX_JOB_THREADS);
    gptData.bRunning = true;
    gptData.uThreadCount = uThreadCount;
    gptAtomics->create_atomic_counter(0, &gptData.ptQueueLatch);
    gptThreads->create_condition_variable(&gptData.ptConditionVariable);
    gptThreads->create_critical_section(&gptData.ptCriticalSection);

    for(uint32_t i = 0; i < PL_MAX_JOBS; i++)
    {
        gptData.atJobs[i].tJob.task = NULL;
        gptData.atJobs[i].tJob.pData = NULL;
        gptAtomics->create_atomic_counter(0, &gptData.atNodes[i].ptCounter);
        gptData.atNodes[i].uNodeIndex = i;
        gptData.atNodes[i].uNextNode = i + 1;
    }
    gptData.uFreeList = 0;

    for(uint32_t i = 0; i < uThreadCount; i++)
        gptThreads->create_thread(pl__thread_procedure, NULL, &gptData.aptThreads[i]);
}

static void
pl__cleanup(void)
{
    gptData.bRunning = false;
    gptThreads->wake_all_condition_variable(gptData.ptConditionVariable);
    for(uint32_t i = 0; i < gptData.uThreadCount; i++)
        gptThreads->join_thread(gptData.aptThreads[i]);

    gptAtomics->destroy_atomic_counter(&gptData.ptQueueLatch);
    gptThreads->destroy_condition_variable(&gptData.ptConditionVariable);
    gptThreads->destroy_critical_section(&gptData.ptCriticalSection);

    for(uint32_t i = 0; i < PL_MAX_JOBS; i++)
    {
        gptData.atJobs[i].tJob.task = NULL;
        gptData.atJobs[i].tJob.pData = NULL;
        gptAtomics->destroy_atomic_counter(&gptData.atNodes[i].ptCounter);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plJobI*
pl_load_job_api(void)
{
    static const plJobI tApi = {
        .initialize       = pl__initialize,
        .cleanup          = pl__cleanup,
        .wait_for_counter = pl__wait_for_counter,
        .run_jobs         = pl__push_jobs_into_queue
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    gptThreads = ptApiRegistry->first(PL_API_THREADS);
    gptAtomics = ptApiRegistry->first(PL_API_ATOMICS);
    if(bReload)
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_JOB), pl_load_job_api());
    else
        ptApiRegistry->add(PL_API_JOB, pl_load_job_api());
}

PL_EXPORT void
pl_unload_ext(plApiRegistryI* ptApiRegistry)
{
    
}
