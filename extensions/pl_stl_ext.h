/*
   pl_stl_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plResourceI (v1.x)
        * plEcsI      (v1.x)
        * plFileI     (v1.x)
        * plVfsI      (v2.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STL_EXT_H
#define PL_STL_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"
#include "pl_ecs_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plStlI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external 
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_stl_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_stl_ext(plApiRegistryI*, bool reload);

PL_API plEntity pl_stl_import(plComponentLibrary*, const char* pcPath, plVec4 tColor, const plMat4* ptTransform);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plStlI
{
    plEntity (*import)(plComponentLibrary*, const char* pcPath, plVec4 tColor, const plMat4* ptTransform);
} plStlI;

#ifdef __cplusplus
}
#endif

#endif // PL_STL_EXT_H