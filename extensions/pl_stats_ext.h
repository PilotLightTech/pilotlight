/*
   pl_stats_ext.h
     - minimal statistics extension
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] apis
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STATS_EXT_H
#define PL_STATS_EXT_H

// extension version (format XYYZZ)
#define PL_STATS_EXT_VERSION    "1.0.0"
#define PL_STATS_EXT_VERSION_NUM 10000

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_STATS "PL_API_STATS"
typedef struct _plStatsI plStatsI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plStatsI
{
    // call once per frame
    void (*new_frame)(void);

    // provides a pointer to a value for the counter
    // (user should store this pointer to prevent lookups every frame)
    double* (*get_counter)(char const* pcName); 

    // provides stat data back to user for analysis/display/etc.
    double**     (*get_counter_data)(char const* pcName); // set point to valid memory
    const char** (*get_names)       (uint32_t* puCount);
    
    // settings
    void     (*set_max_frames)(uint32_t); // default: 120
    uint32_t (*get_max_frames)  (void);
} plStatsI;

#endif // PL_STATS_EXT_H