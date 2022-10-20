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
// [SECTION] global context
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
// [SECTION] global context
//-----------------------------------------------------------------------------

extern plProfileContext* gTPProfileContext;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

#ifdef PL_PROFILE_ON

// setup/shutdown
#define pl_initialize_profile_context(tPContext) pl__initialize_profile_context((tPContext))
#define pl_cleanup_profile_context() pl__cleanup_profile_context()
#define pl_set_profile_context(tPContext) pl__set_profile_context((tPContext))
#define pl_get_profile_context() pl__get_profile_context()

// frames
#define pl_begin_profile_frame(ulFrame) pl__begin_profile_frame(ulFrame)
#define pl_end_profile_frame() pl__end_profile_frame()

// samples
#define pl_begin_profile_sample(cPName) pl__begin_profile_sample(cPName)
#define pl_end_profile_sample() pl__end_profile_sample()

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
void              pl__initialize_profile_context(plProfileContext* tPContext);
void              pl__cleanup_profile_context   (void);
void              pl__set_profile_context       (plProfileContext* tPContext);
plProfileContext* pl__get_profile_context       (void);

// frames
void pl__begin_profile_frame(uint64_t ulFrame);
void pl__end_profile_frame  (void);

// samples
void pl__begin_profile_sample(const char* cPName);
void pl__end_profile_sample  (void);

#ifndef PL_PROFILE_ON
#define pl_initialize_profile_context(tPContext) //
#define pl_cleanup_profile_context() //
#define pl_set_profile_context(tPContext) //
#define pl_get_profile_context() NULL
#define pl_begin_profile_frame(ulFrame) //
#define pl_end_profile_frame() //
#define pl_begin_profile_sample(cPName) //
#define pl_end_profile_sample() //
#endif

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
#include <time.h> // clock_gettime, clock_getres
#endif
#include "pl_ds.h"

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert(x)
#endif

plProfileContext* gTPProfileContext = NULL;

static inline double
pl__get_wall_clock()
{
    #ifdef _WIN32
        INT64 slPerfFrequency = *(INT64*)gTPProfileContext->pInternal;
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
        return (double)nsec_count / *(double*)gTPProfileContext->pInternal;
    #endif
}

void
pl__initialize_profile_context(plProfileContext* tPContext)
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

    gTPProfileContext = tPContext;
    tPContext->dStartTime = pl__get_wall_clock();
    tPContext->sbFrames = NULL;
    tPContext->tPCurrentFrame = &tPContext->tPFrames[0];
}

void
pl__cleanup_profile_context(void)
{
    pl_sb_free(gTPProfileContext->sbFrames);
}

void
pl__set_profile_context(plProfileContext* tPContext)
{
    PL_ASSERT(tPContext && "log context is NULL");
    gTPProfileContext = tPContext;
}

plProfileContext*
pl__get_profile_context(void)
{
    PL_ASSERT(gTPProfileContext && "no global log context set");
    return gTPProfileContext;
}

void
pl__begin_profile_frame(uint64_t ulFrame)
{
    gTPProfileContext->tPCurrentFrame = &gTPProfileContext->tPFrames[ulFrame % 2];
    gTPProfileContext->tPLastFrame = &gTPProfileContext->tPFrames[(ulFrame + 1) % 2];
    
    gTPProfileContext->tPCurrentFrame->ulFrame = ulFrame;
    gTPProfileContext->tPCurrentFrame->dDuration = 0.0;
    gTPProfileContext->tPCurrentFrame->dInternalDuration = 0.0;
    gTPProfileContext->tPCurrentFrame->dStartTime = pl__get_wall_clock();
    pl_sb_reset(gTPProfileContext->tPCurrentFrame->sbSamples);
}

void
pl__end_profile_frame(void)
{
    gTPProfileContext->tPCurrentFrame->dDuration = pl__get_wall_clock() - gTPProfileContext->tPCurrentFrame->dStartTime;
}

void
pl__begin_profile_sample(const char* cPName)
{
    const double dCurrentInternalTime = pl__get_wall_clock();
    plProfileFrame* tPCurrentFrame = gTPProfileContext->tPCurrentFrame;

    plProfileSample tSample = {
        .cPName = cPName,
        .dDuration = 0.0,
        .dStartTime = pl__get_wall_clock(),
        .uDepth = pl_sb_size(tPCurrentFrame->sbSampleStack),
    };

    uint32_t uSampleIndex = pl_sb_size(tPCurrentFrame->sbSamples);
    pl_sb_push(tPCurrentFrame->sbSampleStack, uSampleIndex);
    pl_sb_push(tPCurrentFrame->sbSamples, tSample);
    tPCurrentFrame->dInternalDuration += pl__get_wall_clock() - dCurrentInternalTime;
}

void
pl__end_profile_sample(void)
{
    const double dCurrentInternalTime = pl__get_wall_clock();
    plProfileFrame* tPCurrentFrame = gTPProfileContext->tPCurrentFrame;
    plProfileSample* tPLastSample = &tPCurrentFrame->sbSamples[pl_sb_pop(tPCurrentFrame->sbSampleStack)];
    PL_ASSERT(tPLastSample && "Begin/end profile sampel mismatch");
    tPLastSample->dDuration = pl__get_wall_clock() - tPLastSample->dStartTime;
    tPLastSample->dStartTime -= tPCurrentFrame->dStartTime;
    tPCurrentFrame->dInternalDuration += pl__get_wall_clock() - dCurrentInternalTime;
}

#endif // PL_PROFILE_IMPLEMENTATION