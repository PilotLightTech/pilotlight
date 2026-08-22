/*
   pl_transform_ext.h
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
// [SECTION] components
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plLogI (v1.x)
        * plProfileI (v1.x)
        * plEcsI (v3.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TRANSFORM_EXT_H
#define PL_TRANSFORM_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plTransformI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>      // bool
#include "pl_ecs_ext.inl" // plEntity
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// ecs components
typedef struct _plTransformComponent plTransformComponent;
typedef struct _plHierarchyComponent plHierarchyComponent;

// flags
typedef int plTransformFlags;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_transform_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_transform_ext(plApiRegistryI*, bool reload);

// system setup/shutdown/etc
PL_API void pl_transform_register_ecs_system(void);

// component types (can store)
PL_API plEcsTypeKey pl_transform_get_ecs_type_key_transform(void);
PL_API plEcsTypeKey pl_transform_get_ecs_type_key_hierarchy(void);

// transforms
//   - do NOT store out parameter; use it immediately
PL_API plEntity pl_transform_create_transform(plComponentLibrary*, const char* name, plTransformComponent**);

// hierarchy
PL_API void   pl_transform_attach_component        (plComponentLibrary*, plEntity, plEntity tParent);
PL_API void   pl_transform_detach_component        (plComponentLibrary*, plEntity);
PL_API plMat4 pl_transform_compute_parent_transform(plComponentLibrary*, plEntity);

// systems
PL_API void pl_transform_run_transform_update_system(plComponentLibrary*);
PL_API void pl_transform_run_hierarchy_update_system(plComponentLibrary*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plTransformI
{
    // system setup/shutdown/etc
    void (*register_ecs_system)(void);

    // component types (can store)
    plEcsTypeKey (*get_ecs_type_key_transform)(void);
    plEcsTypeKey (*get_ecs_type_key_hierarchy)(void);

    // transforms
    //   - do NOT store out parameter; use it immediately
    plEntity (*create_transform)(plComponentLibrary*, const char* name, plTransformComponent**);

    // hierarchy
    void   (*attach_component)        (plComponentLibrary*, plEntity, plEntity tParent);
    void   (*detach_component)        (plComponentLibrary*, plEntity);
    plMat4 (*compute_parent_transform)(plComponentLibrary*, plEntity);

    // systems
    void (*run_transform_update_system)(plComponentLibrary*);
    void (*run_hierarchy_update_system)(plComponentLibrary*);

} plTransformI;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plHierarchyComponent
{
    plEntity tParent;
} plHierarchyComponent;

typedef struct _plTransformComponent
{
    plVec3           tScale;
    plVec4           tRotation;
    plVec3           tTranslation;
    plMat4           tWorld;
    plTransformFlags eFlags;
} plTransformComponent;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plTransformFlags
{
    PL_TRANSFORM_FLAGS_NONE  = 0,
    PL_TRANSFORM_FLAGS_DIRTY = 1 << 0,
};

#ifdef __cplusplus
}
#endif

#endif // PL_TRANSFORM_EXT_H