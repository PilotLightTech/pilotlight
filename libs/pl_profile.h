/*
   pl_profile.h
     * simple profiling library
   Do this:
        #define PL_PROFILE_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_PROFILE_IMPLEMENTATION
   #include "pl_profile.h"
*/

// library version (format XYYZZ)
#define PL_PROFILE_VERSION    "1.0.0"
#define PL_PROFILE_VERSION_NUM 10000

/*
Index of this file:
// [SECTION] documentation
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] internal api
// [SECTION] c file start
*/

//-----------------------------------------------------------------------------
// [SECTION] documentation
//-----------------------------------------------------------------------------

/*

SETUP

    pl_create_profile_context:
        plProfileContext* pl_create_profile_context();
            Creates the global context used by the profiling system. Store the
            pointer returned if you want to use the profiler across DLL boundaries.
            See "pl_set_profile_context". 

    pl_cleanup_profile_context:
        void pl_cleanup_profile_context();
            Frees memory associated with the profiling system. Do not call functions
            after this.

    pl_set_profile_context:
        void pl_set_profile_context(plProfileContext*);
            Sets the current log context. Mostly used to allow profiling across
            DLL boundaries.

    pl_get_profile_context:
        plProfileContext* pl_get_profile_context();
            Returns the current profile context.

SAMPLING

    pl_begin_profile_frame:
        void pl_begin_profile_frame();
            Begins a CPU profiling frame. Samples can now be taken.

    pl_end_profile_frame:
        void pl_end_profile_frame();
            Ends a CPU profiling frame.

    pl_begin_profile_sample:
        void pl_begin_profile_sample(pcName);
            Begins a CPU sample. Must have begun a profiling frame.

    pl_end_profile_sample:
        void pl_end_profile_sample();
            Ends a CPU sample.

RETRIEVING RESULTS

    pl_get_last_frame_samples:
        plProfileSample* pl_get_last_frame_samples(uint32_t* puSizeOut);
            Returns samples from last frame. Call after "pl_end_profile_frame".


COMPILE TIME OPTIONS
    * Turn profiling on by defining PL_PROFILE_ON
    * Change allocators by defining both:
        PL_PROFILE_ALLOC(x)
        PL_PROFILE_FREE(x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PROFILE_H
#define PL_PROFILE_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plProfileSample  plProfileSample;  // single sample result
typedef struct _plProfileInit    plProfileInit;    // profile context init info
typedef struct _plProfileContext plProfileContext; // opaque type

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

#ifdef PL_PROFILE_ON

// setup/shutdown
#define pl_create_profile_context(tInit)  pl__create_profile_context((tInit))
#define pl_cleanup_profile_context()      pl__cleanup_profile_context()
#define pl_set_profile_context(ptContext) pl__set_profile_context((ptContext))
#define pl_get_profile_context()          pl__get_profile_context()

// frames
#define pl_begin_profile_frame() pl__begin_profile_frame()
#define pl_end_profile_frame()   pl__end_profile_frame()

// samples
#define pl_begin_profile_sample(uThreadIndex, pcName)   pl__begin_profile_sample((uThreadIndex), (pcName))
#define pl_end_profile_sample(uThreadIndex)             pl__end_profile_sample((uThreadIndex))
#define pl_get_last_frame_samples(uThreadIndex, puSize) pl__get_last_frame_samples((uThreadIndex), (puSize))

#endif // PL_PROFILE_ON

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plProfileSample
{
    double      dStartTime;
    double      dDuration;
    const char* pcName;
    uint32_t    uDepth;
} plProfileSample;

typedef struct _plProfileInit
{
    uint32_t uThreadCount;
} plProfileInit;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// setup/shutdown
plProfileContext* pl__create_profile_context (plProfileInit);
void              pl__cleanup_profile_context(void);
void              pl__set_profile_context    (plProfileContext*);
plProfileContext* pl__get_profile_context    (void);

// frames
void pl__begin_profile_frame(void);
void pl__end_profile_frame  (void);

// samples
void              pl__begin_profile_sample(uint32_t uThreadIndex, const char* pcName);
void              pl__end_profile_sample  (uint32_t uThreadIndex);
plProfileSample*  pl__get_last_frame_samples(uint32_t uThreadIndex, uint32_t* puSizeOut);

#ifndef PL_PROFILE_ON
    #define pl_create_profile_context(ptContext) NULL
    #define pl_cleanup_profile_context() //
    #define pl_set_profile_context(ptContext) //
    #define pl_get_profile_context() NULL
    #define pl_begin_profile_frame(ulFrame) //
    #define pl_end_profile_frame() //
    #define pl_begin_profile_sample(pcName) //
    #define pl_end_profile_sample() //
    #define pl_get_last_frame_samples(puSize) NULL
#endif

#endif // PL_PROFILE_H

//-----------------------------------------------------------------------------
// [SECTION] c file start
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] global context
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifdef PL_PROFILE_IMPLEMENTATION

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

#ifndef PL_PROFILE_ALLOC
    #include <stdlib.h>
    #define PL_PROFILE_ALLOC(x) malloc((x))
    #define PL_PROFILE_FREE(x)  free((x))
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__APPLE__)
    #include <time.h> // clock_gettime_nsec_np
#else // linux
    #include <time.h> // clock_gettime, clock_getres
#endif

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plProfileContext* gptProfileContext = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plProfileFrame
{
    uint64_t         ulFrame;
    double           dStartTime;        // beginning of frame time
    double           dDuration;         // total duration
    double           dInternalDuration; // profiler overhead

    bool             bSampleStackOverflowInUse;
    uint32_t         uTotalSampleStackSize;
    uint32_t*        puSampleStack;
    uint32_t         auSampleStack[256];
    uint32_t         uSampleStackCapacity;

    uint32_t*        puOverflowSampleStack;
    uint32_t         uOverflowSampleStackCapacity;


    uint32_t         uTotalSampleSize;
    plProfileSample* ptSamples;

    bool             bOverflowInUse;
    plProfileSample  atSamples[256];
    uint32_t         uSampleCapacity;
    uint32_t         uOverflowSampleCapacity;
} plProfileFrame;

typedef struct _plProfileThreadData
{
    plProfileFrame  atFrames[2];
    plProfileFrame* ptCurrentFrame;
    plProfileFrame* ptLastFrame;
} plProfileThreadData;

typedef struct _plProfileContext
{
    double               dStartTime;
    uint64_t             ulFrame;
    plProfileThreadData* ptThreadData;
    uint32_t             uThreadCount;
    void*                pInternal;
} plProfileContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static void             pl__push_sample_stack(plProfileFrame* ptFrame, uint32_t uSample);
static plProfileSample* pl__get_sample(plProfileFrame* ptFrame);

static inline uint32_t
pl__pop_sample_stack(plProfileFrame* ptFrame)
{
    ptFrame->uTotalSampleStackSize--;
    return ptFrame->puSampleStack[ptFrame->uTotalSampleStackSize];
}

static inline double
pl__get_wall_clock(void)
{
    double dResult = 0;
    #ifdef _WIN32
        INT64 slPerfFrequency = *(INT64*)gptProfileContext->pInternal;
        INT64 slPerfCounter;
        QueryPerformanceCounter((LARGE_INTEGER*)&slPerfCounter);
        dResult = (double)slPerfCounter / (double)slPerfFrequency;
    #elif defined(__APPLE__)
        dResult = ((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9);
    #else // linux
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
        dResult = (double)nsec_count / *(double*)gptProfileContext->pInternal;
    #endif
    return dResult;
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

plProfileContext*
pl__create_profile_context(plProfileInit tInit)
{
    // allocate context
    plProfileContext* ptContext = (plProfileContext*)PL_PROFILE_ALLOC(sizeof(plProfileContext));
    memset(ptContext, 0, sizeof(plProfileContext));
    gptProfileContext = ptContext;

    // clock setup
    #ifdef _WIN32
        static INT64 slPerfFrequency = 0;
        BOOL bResult = QueryPerformanceFrequency((LARGE_INTEGER*)&slPerfFrequency);
        if(!bResult)
        {
            PL_PROFILE_FREE(gptProfileContext);
            gptProfileContext = NULL;
            return NULL;
        }
        ptContext->pInternal = &slPerfFrequency;
    #elif defined(__APPLE__)
        // no setup required
    #else // linux
        static struct timespec ts;
        if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
        {
            // PL_ASSERT(false && "clock_getres() failed");
            PL_PROFILE_FREE(gptProfileContext);
            gptProfileContext = NULL;
            return NULL;
        }

        static double dPerFrequency = 0.0;
        dPerFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
        ptContext->pInternal = &dPerFrequency;
    #endif

    ptContext->dStartTime = pl__get_wall_clock();
    ptContext->uThreadCount = tInit.uThreadCount;
    ptContext->ptThreadData =  (plProfileThreadData*)PL_PROFILE_ALLOC(sizeof(plProfileThreadData) * tInit.uThreadCount);
    memset(ptContext->ptThreadData, 0, sizeof(plProfileThreadData) * tInit.uThreadCount);
    for(uint32_t i = 0; i < tInit.uThreadCount; i++)
    {
        ptContext->ptThreadData[i].ptCurrentFrame = &ptContext->ptThreadData[i].atFrames[0];
        ptContext->ptThreadData[i].atFrames[0].uSampleCapacity = 256;
        ptContext->ptThreadData[i].atFrames[0].uSampleStackCapacity = 256;
        ptContext->ptThreadData[i].atFrames[1].uSampleCapacity = 256;
        ptContext->ptThreadData[i].atFrames[1].uSampleStackCapacity = 256;
        ptContext->ptThreadData[i].atFrames[0].ptSamples = ptContext->ptThreadData[i].atFrames[0].atSamples;
        ptContext->ptThreadData[i].atFrames[1].ptSamples = ptContext->ptThreadData[i].atFrames[1].atSamples;
        ptContext->ptThreadData[i].atFrames[0].puSampleStack = ptContext->ptThreadData[i].atFrames[0].auSampleStack;
        ptContext->ptThreadData[i].atFrames[1].puSampleStack = ptContext->ptThreadData[i].atFrames[1].auSampleStack;
        ptContext->ptThreadData[i].ptLastFrame = &ptContext->ptThreadData[i].atFrames[0];
    }
    return ptContext;
}

void
pl__cleanup_profile_context(void)
{
    for(uint32_t i = 0; i < gptProfileContext->uThreadCount; i++)
    {
        for(uint32_t j = 0; j < 2; j++)
        {
            
            if(gptProfileContext->ptThreadData[i].atFrames[j].bOverflowInUse)
                PL_PROFILE_FREE(gptProfileContext->ptThreadData[i].atFrames[j].ptSamples);

            if(gptProfileContext->ptThreadData[i].atFrames[j].bSampleStackOverflowInUse)
                PL_PROFILE_FREE(gptProfileContext->ptThreadData[i].atFrames[j].puSampleStack);
        }
    }

    PL_PROFILE_FREE(gptProfileContext->ptThreadData);
    PL_PROFILE_FREE(gptProfileContext);
    gptProfileContext = NULL;
}

void
pl__set_profile_context(plProfileContext* ptContext)
{
    gptProfileContext = ptContext;
}

plProfileContext*
pl__get_profile_context(void)
{
    return gptProfileContext;
}

void
pl__begin_profile_frame(void)
{
    gptProfileContext->ulFrame++;

    for(uint32_t i = 0; i < gptProfileContext->uThreadCount; i++)
    {
        gptProfileContext->ptThreadData[i].ptCurrentFrame = &gptProfileContext->ptThreadData[i].atFrames[gptProfileContext->ulFrame % 2];
        gptProfileContext->ptThreadData[i].ptCurrentFrame->dDuration = 0.0;
        gptProfileContext->ptThreadData[i].ptCurrentFrame->dInternalDuration = 0.0;
        gptProfileContext->ptThreadData[i].ptCurrentFrame->dStartTime = pl__get_wall_clock();
        gptProfileContext->ptThreadData[i].ptCurrentFrame->uTotalSampleSize = 0;
    }
}

void
pl__end_profile_frame(void)
{
    for(uint32_t i = 0; i < gptProfileContext->uThreadCount; i++)
    {
        gptProfileContext->ptThreadData[i].ptCurrentFrame->dDuration = pl__get_wall_clock() - gptProfileContext->ptThreadData[i].ptCurrentFrame->dStartTime;
        gptProfileContext->ptThreadData[i].ptLastFrame = gptProfileContext->ptThreadData[i].ptCurrentFrame;
    }
}

void
pl__begin_profile_sample(uint32_t uThreadIndex, const char* pcName)
{
    const double dCurrentInternalTime = pl__get_wall_clock();
    plProfileFrame* ptCurrentFrame = gptProfileContext->ptThreadData[uThreadIndex].ptCurrentFrame;

    uint32_t uSampleIndex = ptCurrentFrame->uTotalSampleSize;
    plProfileSample* ptSample = pl__get_sample(ptCurrentFrame);
    ptSample->dDuration = 0.0;
    ptSample->dStartTime = pl__get_wall_clock();
    ptSample->pcName = pcName;
    ptSample->uDepth = ptCurrentFrame->uTotalSampleStackSize;

    pl__push_sample_stack(ptCurrentFrame, uSampleIndex);

    ptCurrentFrame->dInternalDuration += pl__get_wall_clock() - dCurrentInternalTime;
}

void
pl__end_profile_sample(uint32_t uThreadIndex)
{
    const double dCurrentInternalTime = pl__get_wall_clock();
    plProfileFrame* ptCurrentFrame = gptProfileContext->ptThreadData[uThreadIndex].ptCurrentFrame;
    plProfileSample* ptLastSample = &ptCurrentFrame->ptSamples[pl__pop_sample_stack(ptCurrentFrame)];
    PL_ASSERT(ptLastSample && "Begin/end profile sample mismatch");
    ptLastSample->dDuration = pl__get_wall_clock() - ptLastSample->dStartTime;
    ptLastSample->dStartTime -= ptCurrentFrame->dStartTime;
    ptCurrentFrame->dInternalDuration += pl__get_wall_clock() - dCurrentInternalTime;
}

plProfileSample*
pl__get_last_frame_samples(uint32_t uThreadIndex, uint32_t* puSize)
{
    plProfileFrame* ptFrame = gptProfileContext->ptThreadData[uThreadIndex].ptLastFrame;

    if(puSize)
        *puSize = ptFrame->uTotalSampleSize;
    return ptFrame->ptSamples;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementations
//-----------------------------------------------------------------------------

static void
pl__push_sample_stack(plProfileFrame* ptFrame, uint32_t uSample)
{
    // check if new overflow
    if(!ptFrame->bSampleStackOverflowInUse && ptFrame->uTotalSampleStackSize == ptFrame->uSampleStackCapacity)
    {
        ptFrame->puOverflowSampleStack = (uint32_t*)PL_PROFILE_ALLOC(sizeof(uint32_t) * ptFrame->uSampleStackCapacity * 2);
        memset(ptFrame->puOverflowSampleStack, 0, sizeof(uint32_t) * ptFrame->uSampleStackCapacity * 2);
        ptFrame->uOverflowSampleStackCapacity = ptFrame->uSampleStackCapacity * 2;

        // copy stack samples
        memcpy(ptFrame->puOverflowSampleStack, ptFrame->auSampleStack, sizeof(uint32_t) * ptFrame->uSampleStackCapacity);
        ptFrame->bSampleStackOverflowInUse = true;
        ptFrame->puSampleStack = ptFrame->puOverflowSampleStack;
    }
    // check if overflow reallocation is needed
    else if(ptFrame->bSampleStackOverflowInUse && ptFrame->uTotalSampleStackSize == ptFrame->uOverflowSampleStackCapacity)
    {
        uint32_t* ptOldOverflowSamples = ptFrame->puOverflowSampleStack;
        ptFrame->puOverflowSampleStack = (uint32_t*)PL_PROFILE_ALLOC(sizeof(uint32_t) * ptFrame->uOverflowSampleStackCapacity * 2);
        memset(ptFrame->puOverflowSampleStack, 0, sizeof(uint32_t) * ptFrame->uOverflowSampleStackCapacity * 2);
        
        // copy old values
        memcpy(ptFrame->puOverflowSampleStack, ptOldOverflowSamples, sizeof(uint32_t) * ptFrame->uOverflowSampleStackCapacity);
        ptFrame->uOverflowSampleStackCapacity *= 2;

        PL_PROFILE_FREE(ptOldOverflowSamples);
        ptFrame->puSampleStack = ptFrame->puOverflowSampleStack;
    }

    ptFrame->puSampleStack[ptFrame->uTotalSampleStackSize] = uSample;
    ptFrame->uTotalSampleStackSize++;
}

static plProfileSample*
pl__get_sample(plProfileFrame* ptFrame)
{
    plProfileSample* ptSample = NULL;

    // check if new overflow
    if(!ptFrame->bOverflowInUse && ptFrame->uTotalSampleSize == ptFrame->uSampleCapacity)
    {
        ptFrame->ptSamples = (plProfileSample*)PL_PROFILE_ALLOC(sizeof(plProfileSample) * ptFrame->uSampleCapacity * 2);
        memset(ptFrame->ptSamples, 0, sizeof(plProfileSample) * ptFrame->uSampleCapacity * 2);
        ptFrame->uOverflowSampleCapacity = ptFrame->uSampleCapacity * 2;

        // copy stack samples
        memcpy(ptFrame->ptSamples, ptFrame->atSamples, sizeof(plProfileSample) * ptFrame->uSampleCapacity);
        ptFrame->bOverflowInUse = true;
    }
    // check if overflow reallocation is needed
    else if(ptFrame->bOverflowInUse && ptFrame->uTotalSampleSize == ptFrame->uOverflowSampleCapacity)
    {
        plProfileSample* ptOldOverflowSamples = ptFrame->ptSamples;
        ptFrame->ptSamples = (plProfileSample*)PL_PROFILE_ALLOC(sizeof(plProfileSample) * ptFrame->uOverflowSampleCapacity * 2);
        memset(ptFrame->ptSamples, 0, sizeof(plProfileSample) * ptFrame->uOverflowSampleCapacity * 2);
        
        // copy old values
        memcpy(ptFrame->ptSamples, ptOldOverflowSamples, sizeof(plProfileSample) * ptFrame->uOverflowSampleCapacity);
        ptFrame->uOverflowSampleCapacity *= 2;

        PL_PROFILE_FREE(ptOldOverflowSamples);
    }

    ptSample = &ptFrame->ptSamples[ptFrame->uTotalSampleSize];
    ptFrame->uTotalSampleSize++;

    return ptSample;
}

#endif // PL_PROFILE_IMPLEMENTATION