/*
   pl_info_ext.h
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

#ifndef PL_INFO_EXT_H
#define PL_INFO_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_INFO "PL_API_INFO"
typedef struct _plInfoI plInfoI;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plInfoI
{
    // check version
    int         (*version)       (void);
    const char* (*version_string)(void);

    // which extension implemented this API
    const char* (*parent_extension)(void);
} plInfoI;

#endif // PL_INFO_EXT_H