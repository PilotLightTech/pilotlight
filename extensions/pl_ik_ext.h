/*
   pl_ik_ext.h
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
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plEcsI       (v3.x)
        * plProfileI   (v2.x)
        * plTransformI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_IK_EXT_H
#define PL_IK_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plIkI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>      // bool
#include "pl_ecs_ext.inl" // plEntity

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// ecs components
typedef struct _plInverseKinematicsComponent plInverseKinematicsComponent;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_ik_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ik_ext(plApiRegistryI*, bool reload);

// system setup/shutdown/etc
PL_API void pl_ik_register_ecs_components(void);

// systems
PL_API void pl_ik_run_update_system(plComponentLibrary*);

// ecs types
PL_API plEcsTypeKey pl_ik_get_ecs_type_key(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plIkI
{
    // system setup/shutdown/etc
    void (*register_ecs_components)(void);

    // systems
    void (*run_ecs_update_system)(plComponentLibrary*);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key)(void);
} plIkI;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plInverseKinematicsComponent
{
    bool     bEnabled;
    plEntity tTarget;
    uint32_t uChainLength;
    uint32_t uIterationCount;
} plInverseKinematicsComponent;

#ifdef __cplusplus
}
#endif

#endif // PL_IK_EXT_H