/*
   pl_job_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_JOB_EXT_H
#define PL_JOB_EXT_H

#define PL_JOB_EXT_VERSION    "1.0.0"
#define PL_JOB_EXT_VERSION_NUM 100000

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_BATCHES
    #define PL_MAX_BATCHES 256
#endif

#ifndef PL_MAX_JOB_THREADS
    #define PL_MAX_JOB_THREADS 64
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define PL_API_JOB "PL_API_JOB"
typedef struct _plJobI plJobI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plJobDesc plJobDesc;

// external
typedef struct _plAtomicCounter plAtomicCounter; // pl_os.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plJobI* pl_load_job_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plJobI
{
    // setup/shutdown
    void (*initialize)(uint32_t uThreadCount); // set thread count to 0 to get optimal thread count
    void (*cleanup)(void);

    // typical usage
    //   - submit an array of job descriptions and receive an atomic counter pointer
    //   - use "wait_for_counter" to wait on jobs to complete and return counter
    void (*dispatch_jobs)(uint32_t uJobCount, plJobDesc*, plAtomicCounter**);

    // batch usage
    //   Follows more of a compute shader design. All jobs use the same data which can be indexed
    //   using the job index. If the jobs are small, consider increasing the group size.
    //   - uJobCount  : how many jobs to generate
    //   - uGroupSize : how many jobs to execute per thread serially (set 0 for optimal group size)
    void (*dispatch_batch)(uint32_t uJobCount, uint32_t uGroupSize, plJobDesc, plAtomicCounter**);
    
    // waits for counter to reach 0 and returns the counter for reuse but subsequent dispatches
    void (*wait_for_counter)(plAtomicCounter*);
} plJobI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plJobDesc
{
    void (*task)(uint32_t uJobIndex, void* pData);
    void* pData;
} plJobDesc;

#endif // PL_JOB_EXT_H