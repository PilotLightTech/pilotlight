/*
   pl_debug_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DEBUG_EXT_H
#define PL_DEBUG_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plDebugApiI_version (plVersion){0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDebugApiI
{
    void (*initialize)(void);
    void (*update)    (void);
} plDebugApiI;

#endif // PL_DEBUG_EXT_H