/*
   pl_skeleton_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] components
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
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include "pl_ecs_ext.inl" // plEntity
#include <stddef.h>       // size_t
#include <stdbool.h>      // bool
#include "pl_math.h"
#include "pl_asset_ext.inl" // plAssetHandle

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plSkeletonJoint plSkeletonJoint;
typedef struct _plSkeleton      plSkeleton;
typedef struct _plSkin          plSkin;

// ecs components
typedef struct _plSkinComponent plSkinComponent;

// external
typedef struct _plComponentLibrary plComponentLibrary;   // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_skeleton_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_skeleton_ext(plApiRegistryI*, bool reload);

//---------------------------ASSET INTEGRATION---------------------------------

PL_API void           pl_skeleton_register_asset_types(void);
PL_API plAssetTypeKey pl_skeleton_get_asset_type_key_skeleton(void);
PL_API plAssetTypeKey pl_skeleton_get_asset_type_key_skin(void);

//----------------------------ECS INTEGRATION----------------------------------

// system setup/shutdown/etc
PL_API void pl_skeleton_ecs_register_system(void);

// systems
PL_API void pl_skeleton_ecs_run_skin_update_system (plComponentLibrary*);

PL_API bool pl_skeleton_ecs_bind_skin(plComponentLibrary*, plEntity, const plEntity* joints, uint32_t jointCount);

// ecs types
PL_API plEcsTypeKey pl_skeleton_ecs_get_type_key_skin (void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plSkeletonI
{

    // assets
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key_skeleton)(void);
    plAssetTypeKey (*get_asset_type_key_skin)(void);

    void (*register_ecs_components)(void);
    
    // ecs system updates
    void (*run_skin_update_system)(plComponentLibrary*);

    bool (*bind_skin)(plComponentLibrary*, plEntity, const plEntity* joints, uint32_t jointCount);

    // editor helpers really
    plEcsTypeKey (*get_ecs_type_key_skin)(void);

} plSkeletonI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plSkeletonJoint
{
    const char* pcName;
    uint32_t    uParent; // UINT32_MAX = root

    // rest/bind local transform
    plVec3 tTranslation; 
    plQuat tRotation;
    plVec3 tScale;
} plSkeletonJoint;

typedef struct _plSkeleton
{
    uint32_t         uJointCount;
    plSkeletonJoint* atJoints;
} plSkeleton;

typedef struct _plSkin
{
    plAssetHandle tSkeleton;

    uint32_t  uJointCount;

    // skin joint index -> skeleton joint index
    //
    // Entry i corresponds to:
    //   vertex JOINTS value i
    //   inverse bind matrix i
    plEntityId* atJoints;

    // one per skin joint, same ordering as auJoints
    plMat4*   atInverseBindMatrices;
} plSkin;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plSkinComponent
{
    plAssetHandle tSkin;
    
    // skin binding
    plAABB tAABB;

    // [INTERNAL]
    uint32_t  _uJointCount;
    plEntity* _atJoints;
    plMat4* _atTextureData;
} plSkinComponent;

#ifdef __cplusplus
}
#endif

#endif // PL_SKELETON_EXT_H