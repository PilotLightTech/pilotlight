/*
   pl_skeleton_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] defines
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
// [SECTION] components
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plEcsI     (v1.x)
        * plProfileI (v1.x)
        * plLogI     (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SKELETON_EXT_H
#define PL_SKELETON_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plSkeletonI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_PATH_LENGTH
    #define PL_MAX_PATH_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stddef.h>       // size_t
#include <stdbool.h>      // bool
#include "pl_math.h" // plEntity
#include "pl_asset_ext.inl" // plAssetHandle

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plSkeletonJoint plSkeletonJoint;
typedef struct _plSkeleton      plSkeleton;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_skeleton_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_skeleton_ext(plApiRegistryI*, bool reload);

PL_API void pl_skeleton_serialize(const char* name, const plSkeleton*);
PL_API void pl_skeleton_deserialize(const char* name, plSkeleton*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plSkeletonI
{

    void (*serialize)  (const char* name, const plSkeleton*);
    void (*deserialize)(const char* name, plSkeleton*);
    
} plSkeletonI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSkeletonJoint
{
    const char* pcName;

    uint32_t uParent;        // UINT32_MAX = root

    plVec3 tTranslation;     // rest/bind local transform
    plQuat tRotation;
    plVec3 tScale;
} plSkeletonJoint;

typedef struct _plSkeleton
{
    uint32_t         uJointCount;
    plSkeletonJoint* atJoints;
} plSkeleton;

#ifdef __cplusplus
}
#endif

#endif // PL_SKELETON_EXT_H