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

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plStatsI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plStatsI
{
    // call once per frame
    void (*new_frame)(void);

    // provides a pointer to a value for the counter
    // (user should store this pointer to prevent lookups every frame)
    double* (*get_counter)(char const* name); 

    // provides stat data back to user for analysis/display/etc.
    double**     (*get_counter_data)(char const* name); // set point to valid memory
    const char** (*get_names)       (uint32_t* countOut);
    
    // settings
    void     (*set_max_frames)(uint32_t); // default: 120
    uint32_t (*get_max_frames)  (void);
} plStatsI;

#endif // PL_STATS_EXT_H