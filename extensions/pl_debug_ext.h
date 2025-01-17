/*
   pl_debug_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] apis
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DEBUG_EXT_H
#define PL_DEBUG_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plDebugApiI_version (plVersion){0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plDebugApiInfo plDebugApiInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDebugApiI
{
    void (*show_debug_windows)(plDebugApiInfo*);
} plDebugApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDebugApiInfo
{
    bool bShowDeviceMemoryAnalyzer;
    bool bShowMemoryAllocations;
    bool bShowProfiling;
    bool bShowStats;
    bool bShowLogging;
} plDebugApiInfo;

#endif // PL_DEBUG_EXT_H