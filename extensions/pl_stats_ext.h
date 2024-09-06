/*
   pl_stats_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
// [SECTION] apis
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STATS_EXT_H
#define PL_STATS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_STATS_MAX_FRAMES
    #define PL_STATS_MAX_FRAMES 120
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_STATS "PL_API_STATS"
typedef struct _plStatsI plStatsI;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plStatsI
{
    // call once per frame
    void (*new_frame)(void);

    // provides a pointer to a value for the counter
    // (user should store this pointer to prevent lookups every frame)
    double* (*get_counter)(char const* pcName); 

    // provides stat data back to user for analysis/display/etc.
    double**     (*get_counter_data)(char const* pcName);
    const char** (*get_names)       (uint32_t* puCount);
} plStatsI;

#endif // PL_STATS_EXT_H