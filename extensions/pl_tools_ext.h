/*
   pl_tools_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] forward declarations & basic types
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TOOLS_EXT_H
#define PL_TOOLS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plToolsI_version (plVersion){1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plToolsInit plToolsInit;

// external
typedef struct _plDevice plDevice; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plToolsI
{
    void (*initialize)(plToolsInit);
    void (*update)    (void); // call after beginning ui frame
} plToolsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plToolsInit
{
    plDevice* ptDevice;
} plToolsInit;

#endif // PL_TOOLS_EXT_H