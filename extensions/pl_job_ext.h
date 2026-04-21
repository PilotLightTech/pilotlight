/*
   pl_job_ext.h
     - simple job system based on compute shaders
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plAtomicsI (v1.x)
        * plThreadsI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_JOB_EXT_H
#define PL_JOB_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>  // uint32_t
#include <stddef.h>  // size_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plJobI_version {2, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plJobSystemInit  plJobSystemInit;
typedef struct _plJobDesc        plJobDesc;
typedef struct _plInvocationData plInvocationData;

// external
typedef struct _plAtomicCounter plAtomicCounter; // pl_os.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_job_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_job_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void pl_job_initialize(plJobSystemInit); 
PL_API void pl_job_cleanup   (void);

// typical usage
//   - submit an array of job descriptions and receive an atomic counter pointer
//   - pass NULL for the atomic counter pointer if you don't need to wait (fire & forget)
//   - use "wait_for_counter" to wait on jobs to complete and return counter for reuse
PL_API void pl_job_dispatch_jobs(uint32_t jobCount, plJobDesc*, plAtomicCounter**);

// batch usage
//   Follows more of a compute shader design. All jobs use the same data which can be indexed
//   using the job index. If the jobs are small, consider increasing the group size.
//   - jobCount  : how many jobs to generate
//   - groupSize : how many jobs to execute per thread serially (set 0 for optimal group size)
//   - pass NULL for the atomic counter pointer if you don't need to wait (fire & forget)
PL_API void pl_job_dispatch_batch(uint32_t jobCount, uint32_t groupSize, plJobDesc, plAtomicCounter**);

// waits for counter to reach 0 and returns the counter for reuse by subsequent dispatches
PL_API void pl_job_wait_for_counter(plAtomicCounter*);

// long running jobs should check this & exit themselves
PL_API bool pl_job_is_shutting_down(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plJobI
{
    void (*initialize)      (plJobSystemInit); 
    void (*cleanup)         (void);
    void (*dispatch_jobs)   (uint32_t jobCount, plJobDesc*, plAtomicCounter**);
    void (*dispatch_batch)  (uint32_t jobCount, uint32_t groupSize, plJobDesc, plAtomicCounter**);
    void (*wait_for_counter)(plAtomicCounter*);
    bool (*is_shutting_down)(void);
} plJobI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plJobSystemInit
{
    uint32_t uThreadCount;       // set thread count to 0 to get optimal thread count
    size_t   szSharedMemorySize; // default: 0
} plJobSystemInit;

typedef struct _plInvocationData
{
    uint32_t uBatchIndex;
    uint32_t uLocalIndex;        // index within batch
    uint32_t uGlobalIndex;       // job index (uBatchIndex + uLocalIndex)
    uint32_t uBatchSize;         // size of current batch
    size_t   szSharedMemorySize; // size of groupSharedMemory
} plInvocationData;

typedef struct _plJobDesc
{
    void (*task)(plInvocationData, void* data, void* groupSharedMemory); // NOTE: groupSharedMemory is not zeroed
    void* pData;
} plJobDesc;

#ifdef __cplusplus
}
#endif

#endif // PL_JOB_EXT_H