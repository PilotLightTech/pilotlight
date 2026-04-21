/*
   pl_physics_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plDrawI    (v1.x)
        * plProfileI (v1.x)
        * plLogI     (v1.x)
        * plStatsI   (v1.x)

        unstable APIs:
        * plCollisionI
        * plEcsI
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PHYSICS_EXT_H
#define PL_PHYSICS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plPhysicsI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plPhysicsEngineSettings plPhysicsEngineSettings;

// ecs components
typedef struct _plRigidBodyPhysicsComponent plRigidBodyPhysicsComponent;
typedef struct _plForceFieldComponent       plForceFieldComponent;

// enums & flags
typedef int plCollisionShape;
typedef int plRigidBodyPhysicsFlags;
typedef int plForceFieldType;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef uint32_t                   plEcsTypeKey;       // pl_ecs_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h
typedef union  _plVec3             plVec3;             // pl_math.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_physics_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_physics_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void                    pl_physics_initialize(plPhysicsEngineSettings);
PL_API void                    pl_physics_cleanup   (void);
PL_API void                    pl_physics_reset     (void);

// setttings
PL_API void pl_physics_set_settings(plPhysicsEngineSettings);
PL_API void pl_physics_get_settings(plPhysicsEngineSettings* out);

// per frame
PL_API void                    pl_physics_update(float deltaTime, plComponentLibrary*);
PL_API void                    pl_physics_draw  (plComponentLibrary*, plDrawList3D*);

// forces/torques/impulses
PL_API void                    pl_physics_apply_force                (plComponentLibrary*, plEntity, plVec3);
PL_API void                    pl_physics_apply_force_at_point       (plComponentLibrary*, plEntity, plVec3, plVec3 point);
PL_API void                    pl_physics_apply_force_at_body_point  (plComponentLibrary*, plEntity, plVec3, plVec3 point);
PL_API void                    pl_physics_apply_impulse              (plComponentLibrary*, plEntity, plVec3);
PL_API void                    pl_physics_apply_impulse_at_point     (plComponentLibrary*, plEntity, plVec3, plVec3 point);
PL_API void                    pl_physics_apply_impulse_at_body_point(plComponentLibrary*, plEntity, plVec3, plVec3 point);
PL_API void                    pl_physics_apply_torque               (plComponentLibrary*, plEntity, plVec3);
PL_API void                    pl_physics_apply_impulse_torque       (plComponentLibrary*, plEntity, plVec3);

// velocities
PL_API void                    pl_physics_set_linear_velocity  (plComponentLibrary*, plEntity, plVec3);
PL_API void                    pl_physics_set_angular_velocity (plComponentLibrary*, plEntity, plVec3);

// ops
PL_API void                    pl_physics_wake_up_body(plComponentLibrary*, plEntity);
PL_API void                    pl_physics_wake_up_all (void);
PL_API void                    pl_physics_sleep_body  (plComponentLibrary*, plEntity);
PL_API void                    pl_physics_sleep_all   (void);

// misc.
PL_API void                   pl_physics_create_rigid_body(plComponentLibrary*, plEntity);

//----------------------------ECS INTEGRATION----------------------------------

// system setup/shutdown/etc
PL_API void                   pl_physics_register_ecs_system(void);

// ecs types
PL_API plEcsTypeKey           pl_physics_get_ecs_type_key_rigid_body_physics(void);
PL_API plEcsTypeKey           pl_physics_get_ecs_type_key_force_field       (void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plPhysicsI
{
    // setup/shutdown
    void (*initialize)(plPhysicsEngineSettings);
    void (*cleanup)(void);
    void (*reset)(void);

    // setttings
    void (*set_settings)(plPhysicsEngineSettings);
    void (*get_settings)(plPhysicsEngineSettings* out);

    // per frame
    void (*update)(float deltaTime, plComponentLibrary*);
    void (*draw)(plComponentLibrary*, plDrawList3D*);

    // forces/torques/impulses
    void (*apply_force)                (plComponentLibrary*, plEntity, plVec3);
    void (*apply_force_at_point)       (plComponentLibrary*, plEntity, plVec3, plVec3 point);
    void (*apply_force_at_body_point)  (plComponentLibrary*, plEntity, plVec3, plVec3 point);
    void (*apply_impulse)              (plComponentLibrary*, plEntity, plVec3);
    void (*apply_impulse_at_point)     (plComponentLibrary*, plEntity, plVec3, plVec3 point);
    void (*apply_impulse_at_body_point)(plComponentLibrary*, plEntity, plVec3, plVec3 point);
    void (*apply_torque)               (plComponentLibrary*, plEntity, plVec3);
    void (*apply_impulse_torque)       (plComponentLibrary*, plEntity, plVec3);

    // velocities
    void (*set_linear_velocity)  (plComponentLibrary*, plEntity, plVec3);
    void (*set_angular_velocity) (plComponentLibrary*, plEntity, plVec3);

    // ops
    void (*wake_up_body)(plComponentLibrary*, plEntity);
    void (*wake_up_all) (void);
    void (*sleep_body)  (plComponentLibrary*, plEntity);
    void (*sleep_all)   (void);

    // misc.
    void (*create_rigid_body)(plComponentLibrary*, plEntity);

    //----------------------------ECS INTEGRATION----------------------------------

    // system setup/shutdown/etc
    void (*register_ecs_system)(void);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key_rigid_body_physics)(void);
    plEcsTypeKey (*get_ecs_type_key_force_field)       (void);

} plPhysicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plPhysicsEngineSettings
{
    bool     bEnabled;               // default: false
    float    fSleepEpsilon;          // default: 0.5f
    float    fPositionEpsilon;       // default: 0.01f
    float    fVelocityEpsilon;       // default: 0.01f
    uint32_t uMaxPositionIterations; // default: 256
    uint32_t uMaxVelocityIterations; // default: 256
    float    fSimulationMultiplier;  // default: 1.0f
    float    fSimulationFrameRate;   // default: 60.0f;
} plPhysicsEngineSettings;

typedef struct _plRigidBodyPhysicsComponent
{
    plRigidBodyPhysicsFlags tFlags;
    plCollisionShape        tShape;
    float                   fMass;
    float                   fRestitution;
    float                   fLinearDamping;
    float                   fAngularDamping;
    float                   fFriction;
    plVec3                  tGravity;
    plVec3                  tLocalOffset;

    // sphere
    float fRadius;

    // box
    plVec3 tExtents;

    uint64_t uPhysicsObject;
} plRigidBodyPhysicsComponent;

typedef struct _plForceFieldComponent
{
    plForceFieldType tType;
    float            fGravity;
    float            fRange;
} plForceFieldComponent;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plCollisionShape
{
    PL_COLLISION_SHAPE_BOX,
    PL_COLLISION_SHAPE_SPHERE,
    // PL_COLLISION_SHAPE_CAPSULE,
    // PL_COLLISION_SHAPE_CONVEX_HULL,
    // PL_COLLISION_SHAPE_TRIANGULAR_MESH,
    // PL_COLLISION_SHAPE_CYLINDER
};

enum _plRigidBodyPhysicsFlags
{
    PL_RIGID_BODY_PHYSICS_FLAG_NONE           = 0,
    PL_RIGID_BODY_PHYSICS_FLAG_START_SLEEPING = 1 << 0,
    PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING    = 1 << 1,
    PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC      = 1 << 2,
};

enum _plForceFieldType
{
    PL_FORCE_FIELD_TYPE_POINT,
    PL_FORCE_FIELD_TYPE_PLANE
};

#ifdef __cplusplus
}
#endif

#endif // PL_PHYSICS_EXT_H