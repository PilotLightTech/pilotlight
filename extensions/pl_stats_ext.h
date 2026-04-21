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
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STATS_EXT_H
#define PL_STATS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plStatsI_version {1, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_stats_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_stats_ext(plApiRegistryI*, bool reload);

// call once per frame
PL_API void         pl_stats_new_frame        (void);

// provides a pointer to a value for the counter
// (user should store this pointer to prevent lookups every frame)
PL_API double*      pl_stats_get_counter      (char const* name); 

// provides stat data back to user for analysis/display/etc.
PL_API double**     pl_stats_get_counter_data(char const* name); // set point to valid memory
PL_API const char** pl_stats_get_names       (uint32_t* countOut);

// settings
PL_API void         pl_stats_set_max_frames  (uint32_t); // default: 120
PL_API uint32_t     pl_stats_get_max_frames  (void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plStatsI
{
    void         (*new_frame)       (void);
    double*      (*get_counter)     (char const* name); 
    double**     (*get_counter_data)(char const* name);
    const char** (*get_names)       (uint32_t* countOut);
    void         (*set_max_frames)  (uint32_t);
    uint32_t     (*get_max_frames)  (void);
} plStatsI;

#ifdef __cplusplus
}
#endif

#endif // PL_STATS_EXT_H