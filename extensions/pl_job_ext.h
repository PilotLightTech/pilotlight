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

#ifndef PL_MAX_JOBS
    #define PL_MAX_JOBS 64
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
    void (*initialize)(uint32_t uThreadCount);
    void (*cleanup)(void);

    // typical usage
    void (*run_jobs)        (plJobDesc* ptJobs, uint32_t uJobCount, plAtomicCounter** pptCounter);
    void (*wait_for_counter)(plAtomicCounter* ptCounter, uint32_t uValue);
} plJobI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plJobDesc
{
    void (*task)(void* pData);
    void* pData;
} plJobDesc;

#endif // PL_JOB_EXT_H