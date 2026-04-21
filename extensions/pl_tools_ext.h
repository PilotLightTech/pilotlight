/*
   pl_tools_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plGraphicsI      (v1.x)
        * plStatsI         (v1.x)
        * plGPUAllocatorsI (v1.x)
        * plDrawI          (v1.x)
        * plUiI            (v1.x)
        * plProfileI       (v1.x)
        * plLogI           (v1.x)
        * plConsoleI       (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TOOLS_EXT_H
#define PL_TOOLS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plToolsI_version {1, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plToolsInit plToolsInit;

// external
typedef struct _plDevice plDevice; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_tools_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_tools_ext(plApiRegistryI*, bool reload);

PL_API void pl_tools_initialize(plToolsInit);
PL_API void pl_tools_update    (void); // call after beginning ui frame

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plToolsI
{
    void (*initialize)(plToolsInit);
    void (*update)    (void);
} plToolsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plToolsInit
{
    plDevice* ptDevice;
} plToolsInit;

#ifdef __cplusplus
}
#endif

#endif // PL_TOOLS_EXT_H