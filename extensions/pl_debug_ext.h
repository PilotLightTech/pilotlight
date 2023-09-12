/*
   pl_debug_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] apis
// [SECTION] forward declarations
// [SECTION] public api
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

#define PL_API_DEBUG "PL_API_DEBUG"
typedef struct _plDebugApiI plDebugApiI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plDebugApiInfo plDebugApiInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plDebugApiI* pl_load_debug_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDebugApiI
{
    void (*show_windows)(plDebugApiInfo* ptInfo);
} plDebugApiI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDebugApiInfo
{
    // bool bShowDeviceMemoryAnalyzer;
    bool bShowMemoryAllocations;
    bool bShowProfiling;
    bool bShowStats;
    bool bShowLogging;
} plDebugApiInfo;

#endif // PL_DEBUG_EXT_H