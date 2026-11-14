/*
   pl_stl_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plAssetI (v1.x)
        * plMemoryI (v1.x)
        * plMeshI (v0.3.x)
        * plVfsI (v3.x)
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
#include <stdbool.h>
#include "pl_asset_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plStlI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_stl_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_stl_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plStlI
{
    plAssetHandle (*import)(const char* pcPath);
} plStlI;

#ifdef __cplusplus
}
#endif

#endif // PL_STL_EXT_H