/*
   pl_profile_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
// [SECTION] macros
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PROFILE_EXT_H
#define PL_PROFILE_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plProfileI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plProfileCpuSample plProfileCpuSample;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plProfileI
{
    // frames
    void (*begin_frame)(void);
    void (*end_frame)  (void);

    // sampling (prefer macros below)
    void (*begin_sample)(uint32_t threadIndex, const char* name);
    void (*end_sample)  (uint32_t threadIndex);

    // results
    plProfileCpuSample* (*get_last_frame_samples) (uint32_t threadIndex, uint32_t* sizeOut);
    double              (*get_last_frame_overhead)(uint32_t threadIndex);

} plProfileI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plProfileCpuSample
{
    double      dStartTime;
    double      dDuration;
    const char* pcName;
    uint32_t    _uDepth;
} plProfileCpuSample;

//-----------------------------------------------------------------------------
// [SECTION] macros
//-----------------------------------------------------------------------------

#ifdef PL_PROFILE_ON
    #define pl_begin_cpu_sample(api, uThreadIndex, pcName) (api)->begin_sample((uThreadIndex), (pcName))
    #define pl_end_cpu_sample(api, uThreadIndex)           (api)->end_sample((uThreadIndex))
#else
    #define pl_begin_cpu_sample(api, uThreadIndex, pcName) //
    #define pl_end_cpu_sample(api, uThreadIndex) //
#endif

#endif // PL_PROFILE_EXT_H