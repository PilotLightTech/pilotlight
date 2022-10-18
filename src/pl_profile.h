/*
   pl_profile, v0.1 (WIP)
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

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] internal api
// [SECTION] implementation
*/

#ifndef PL_PROFILE_H
#define PL_PROFILE_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_DECLARE_STRUCT
#define PL_DECLARE_STRUCT(name) typedef struct name ##_t name
#endif

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
PL_DECLARE_STRUCT(plProfileSample);
PL_DECLARE_STRUCT(plProfileFrame);
PL_DECLARE_STRUCT(plProfileContext);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

#ifdef PL_PROFILE_ON

// setup/shutdown
#define pl_create_profile_context(tPContext) pl__create_profile_context(tPContext)
#define pl_cleanup_profile_context(tPContext) pl__cleanup_profile_context(tPContext)

// frames
#define pl_begin_profile_frame(tPContext, ulFrame) pl__begin_profile_frame(tPContext, ulFrame)
#define pl_end_profile_frame(tPContext) pl__end_profile_frame(tPContext)

// samples
#define pl_begin_profile_sample(tPContext, cPName) pl__begin_profile_sample(tPContext, cPName)
#define pl_end_profile_sample(tPContext) pl__end_profile_sample(tPContext)

#else
#define pl_create_profile_context(tPContext) //
#define pl_cleanup_profile_context(tPContext) //
#define pl_begin_profile_frame(tPContext, ulFrame) //
#define pl_end_profile_frame(tPContext) //
#define pl_begin_profile_sample(tPContext, cPName) //
#define pl_end_profile_sample(tPContext) //
#endif // PL_PROFILE_ON

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plProfileSample_t
{
    double      dStartTime;
    double      dDuration;
    const char* cPName;
    uint32_t    uDepth;
} plProfileSample;

typedef struct plProfileFrame_t
{
    uint32_t*        sbSampleStack;
    plProfileSample* sbSamples;
    uint64_t         ulFrame;
    double           dStartTime;
    double           dDuration;
    double           dInternalDuration;
} plProfileFrame;

typedef struct plProfileContext_t
{
    double          dStartTime;
    plProfileFrame* sbFrames;
    plProfileFrame  tPFrames[2];
    plProfileFrame* tPCurrentFrame;
    plProfileFrame* tPLastFrame;
    void*           pInternal;
} plProfileContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// setup/shutdown
void pl__create_profile_context (plProfileContext* tPContext);
void pl__cleanup_profile_context(plProfileContext* tPContext);

// frames
void pl__begin_profile_frame(plProfileContext* tPContext, uint64_t ulFrame);
void pl__end_profile_frame  (plProfileContext* tPContext);

// samples
void pl__begin_profile_sample(plProfileContext* tPContext, const char* cPName);
void pl__end_profile_sample  (plProfileContext* tPContext);

#endif // PL_PROFILE_H

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

#ifdef PL_PROFILE_IMPLEMENTATION

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE
#define NOGDICAPMASKS       // - CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOSYSMETRICS        // - SM_*
#define NOMENUS             // - MF_*
#define NOICONS             // - IDI_*
#define NOSYSCOMMANDS       // - SC_*
#define NORASTEROPS         // - Binary and Tertiary raster ops
#define OEMRESOURCE         // - OEM Resource values
#define NOATOM              // - Atom Manager routines
#define NOCLIPBOARD         // - Clipboard routines
#define NOCOLOR             // - Screen colors
#define NODRAWTEXT          // - DrawText() and DT_*
#define NOMEMMGR            // - GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE          // - typedef METAFILEPICT
#define NOMINMAX            // - Macros min(a,b) and max(a,b)
#define NOOPENFILE          // - OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL            // - SB_* and scrolling routines
#define NOSERVICE           // - All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND             // - Sound driver routines
#define NOTEXTMETRIC        // - typedef TEXTMETRIC and associated routines
#define NOCOMM              // - COMM driver routines
#define NOKANJI             // - Kanji support stuff.
#define NOHELP              // - Help engine interface.
#define NOPROFILER          // - Profiler interface.
#define NODEFERWINDOWPOS    // - DeferWindowPos routines
#define NOMCX               // - Modem Configuration Extensions
#include <windows.h>
#elif defined(__APPLE__)
#include <time.h> // clock_gettime_nsec_np
#else // linux
#define __USE_POSIX199309
#include <time.h> // clock_gettime, clock_getres
#undef __USE_POSIX199309
#endif
#include "pl_ds.h"

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert(x)
#endif


static inline double
pl__get_wall_clock(plProfileContext* tPContext)
{
    #ifdef _WIN32
    INT64 slPerfFrequency = *(INT64*)tPContext->pInternal;
    INT64 slPerfCounter;
    QueryPerformanceCounter((LARGE_INTEGER*)&slPerfCounter);
    return(double)slPerfCounter / (double)slPerfFrequency;
    #elif defined(__APPLE__)
    return ((double)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1e9);
    #else // linux
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) 
    {
        PL_ASSERT(false && "clock_gettime() failed");
    }
    uint64_t nsec_count = ts.tv_nsec + ts.tv_sec * 1e9;
    return (double)nsec_count / *(double*)tPContext->pInternal;
    #endif
}

void
pl__create_profile_context(plProfileContext* tPContext)
{

    #ifdef _WIN32
    static INT64 slPerfFrequency = 0;
    PL_ASSERT(QueryPerformanceFrequency((LARGE_INTEGER*)&slPerfFrequency));
    tPContext->pInternal = &slPerfFrequency;
    #elif defined(__APPLE__)
    #else // linux
    static struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) 
    {
        PL_ASSERT(false && "clock_getres() failed");
    }

    static double dPerFrequency = 0.0;
    dPerFrequency = 1e9/((double)ts.tv_nsec + (double)ts.tv_sec * (double)1e9);
    tPContext->pInternal = &dPerFrequency;
    #endif

    tPContext->dStartTime = pl__get_wall_clock(tPContext);
    tPContext->sbFrames = NULL;
    tPContext->tPCurrentFrame = &tPContext->tPFrames[0];
}

void
pl__cleanup_profile_context(plProfileContext* tPContext)
{
    pl_sb_free(tPContext->sbFrames);
}

void
pl__begin_profile_frame(plProfileContext* tPContext, uint64_t ulFrame)
{
    tPContext->tPCurrentFrame = &tPContext->tPFrames[ulFrame % 2];
    tPContext->tPLastFrame = &tPContext->tPFrames[(ulFrame + 1) % 2];
    
    tPContext->tPCurrentFrame->ulFrame = ulFrame;
    tPContext->tPCurrentFrame->dDuration = 0.0;
    tPContext->tPCurrentFrame->dInternalDuration = 0.0;
    tPContext->tPCurrentFrame->dStartTime = pl__get_wall_clock(tPContext);
    pl_sb_reset(tPContext->tPCurrentFrame->sbSamples);
}

void
pl__end_profile_frame(plProfileContext* tPContext)
{
    tPContext->tPCurrentFrame->dDuration = pl__get_wall_clock(tPContext) - tPContext->tPCurrentFrame->dStartTime;
}

void
pl__begin_profile_sample(plProfileContext* tPContext, const char* cPName)
{
    const double dCurrentInternalTime = pl__get_wall_clock(tPContext);
    plProfileFrame* tPCurrentFrame = tPContext->tPCurrentFrame;

    plProfileSample tSample = {
        .cPName = cPName,
        .dDuration = 0.0,
        .dStartTime = pl__get_wall_clock(tPContext),
        .uDepth = pl_sb_size(tPCurrentFrame->sbSampleStack),
    };

    uint32_t uSampleIndex = pl_sb_size(tPCurrentFrame->sbSamples);
    pl_sb_push(tPCurrentFrame->sbSampleStack, uSampleIndex);
    pl_sb_push(tPCurrentFrame->sbSamples, tSample);
    tPCurrentFrame->dInternalDuration += pl__get_wall_clock(tPContext) - dCurrentInternalTime;
}

void
pl__end_profile_sample(plProfileContext* tPContext)
{
    const double dCurrentInternalTime = pl__get_wall_clock(tPContext);
    plProfileFrame* tPCurrentFrame = tPContext->tPCurrentFrame;
    plProfileSample* tPLastSample = &tPCurrentFrame->sbSamples[pl_sb_pop(tPCurrentFrame->sbSampleStack)];
    PL_ASSERT(tPLastSample && "Begin/end profile sampel mismatch");
    tPLastSample->dDuration = pl__get_wall_clock(tPContext) - tPLastSample->dStartTime;
    tPLastSample->dStartTime -= tPCurrentFrame->dStartTime;
    tPCurrentFrame->dInternalDuration += pl__get_wall_clock(tPContext) - dCurrentInternalTime;
}

#endif // PL_PROFILE_IMPLEMENTATION