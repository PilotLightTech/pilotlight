/*
   pl_job_ext.h
     - simple job system extension
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_JOB_EXT_H
#define PL_JOB_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plJobI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plJobDesc        plJobDesc;
typedef struct _plInvocationData plInvocationData;

// external
typedef struct _plAtomicCounter plAtomicCounter; // pl_os.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plJobI
{
    // setup/shutdown
    void (*initialize)(uint32_t threadCount); // set thread count to 0 to get optimal thread count
    void (*cleanup)(void);

    // typical usage
    //   - submit an array of job descriptions and receive an atomic counter pointer
    //   - pass NULL for the atomic counter pointer if you don't need to wait (fire & forget)
    //   - use "wait_for_counter" to wait on jobs to complete and return counter for reuse
    void (*dispatch_jobs)(uint32_t jobCount, plJobDesc*, plAtomicCounter**);

    // batch usage
    //   Follows more of a compute shader design. All jobs use the same data which can be indexed
    //   using the job index. If the jobs are small, consider increasing the group size.
    //   - jobCount  : how many jobs to generate
    //   - groupSize : how many jobs to execute per thread serially (set 0 for optimal group size)
    //   - pass NULL for the atomic counter pointer if you don't need to wait (fire & forget)
    void (*dispatch_batch)(uint32_t jobCount, uint32_t groupSize, plJobDesc, plAtomicCounter**);
    
    // waits for counter to reach 0 and returns the counter for reuse by subsequent dispatches
    void (*wait_for_counter)(plAtomicCounter*);
} plJobI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plInvocationData
{
    uint32_t uBatchIndex;
    uint32_t uLocalIndex;
    uint32_t uGlobalIndex;
    uint32_t uBatchSize;
} plInvocationData;

typedef struct _plJobDesc
{
    void (*task)(plInvocationData, void* data);
    void* pData;
} plJobDesc;

#endif // PL_JOB_EXT_H