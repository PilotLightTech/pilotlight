/*
   pl_stats_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] defines
// [SECTION] apis
// [SECTION] public api
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
typedef struct _plStatsApiI plStatsApiI;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plStatsApiI* pl_load_stats_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plStatsApiI
{
    double**     (*get_counter_data)(char const* pcName);
    double*      (*get_counter)     (char const* pcName); // provided a pointer to a value for the counter
    void         (*new_frame)       (void);
    const char** (*get_names)       (uint32_t* puCount);
} plStatsApiI;

#endif // PL_STATS_EXT_H