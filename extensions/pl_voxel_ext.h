/*
   pl_voxel_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plDataRegistryI (v1.x)
        * plMemoryI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_VOXEL_EXT_H
#define PL_VOXEL_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plVoxelI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plVoxelI
{
    int _iUnused;

} plVoxelI;

#ifdef __cplusplus
}
#endif

#endif // PL_VOXEL_EXT_H