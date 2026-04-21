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
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] macros
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PROFILE_EXT_H
#define PL_PROFILE_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plProfileI_version {2, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plProfileCpuSample plProfileCpuSample;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_profile_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_profile_ext(plApiRegistryI*, bool reload);

// frames
PL_API void pl_profile_begin_frame(void);
PL_API void pl_profile_end_frame  (void);

// sampling (prefer macros below)
PL_API void pl_profile_begin_sample(uint32_t threadIndex, const char* name);
PL_API void pl_profile_end_sample  (uint32_t threadIndex);

// results
PL_API plProfileCpuSample* pl_profile_get_last_frame_samples (uint32_t threadIndex, uint32_t* sizeOut);
PL_API double              pl_profile_get_last_frame_overhead(uint32_t threadIndex);

// misc.
PL_API void pl_profile_set_thread_count(uint32_t threadCount);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
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

    // misc.
    void (*set_thread_count)(uint32_t threadCount); // default is 4, used for indexing
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
    #define PL_PROFILE_BEGIN_SAMPLE_API(api, uThreadIndex, pcName) (api)->begin_sample((uThreadIndex), (pcName))
    #define PL_PROFILE_END_SAMPLE_API(api, uThreadIndex)           (api)->end_sample((uThreadIndex))
    #define PL_PROFILE_BEGIN_SAMPLE(uThreadIndex, pcName) pl_profile_begin_sample((uThreadIndex), (pcName))
    #define PL_PROFILE_END_SAMPLE(uThreadIndex)           pl_profile_end_sample((uThreadIndex))
#else
    #define PL_PROFILE_BEGIN_SAMPLE_API(api, uThreadIndex, pcName) //
    #define PL_PROFILE_END_SAMPLE_API(api, uThreadIndex) //
    #define PL_PROFILE_BEGIN_SAMPLE(uThreadIndex, pcName) //
    #define PL_PROFILE_END_SAMPLE(uThreadIndex) //
#endif

#ifdef __cplusplus
}
#endif

#endif // PL_PROFILE_EXT_H