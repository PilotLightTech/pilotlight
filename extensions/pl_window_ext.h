/*
   pl_window_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_WINDOW_EXT_H
#define PL_WINDOW_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plWindowI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plWindow     plWindow;
typedef struct _plWindowDesc plWindowDesc;

// enums
typedef int plWindowResult; // -> enum _plWindowResult // Enum:

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plWindowI
{

    plWindowResult (*create_window) (plWindowDesc, plWindow** windowPtrOut);
    void           (*destroy_window)(plWindow*);
    
} plWindowI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plWindowResult
{
    PL_WINDOW_RESULT_FAIL    = 0,
    PL_WINDOW_RESULT_SUCCESS = 1
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plWindowDesc
{
    const char* pcTitle;
    uint32_t    uWidth;
    uint32_t    uHeight;
    int         iXPos;
    int         iYPos;
    const void* pNext;
} plWindowDesc;

typedef struct _plWindow
{
    plWindowDesc tDesc;
    void*        _pPlatformData;
} plWindow;

#endif // PL_WINDOW_EXT_H