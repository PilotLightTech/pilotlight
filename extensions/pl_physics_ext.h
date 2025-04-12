/*
   pl_physics_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PHYSICS_EXT_H
#define PL_PHYSICS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plPhysicsI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plPhysicsEngineSettings plPhysicsEngineSettings;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h
typedef union  _plVec3             plVec3;             // pl_math.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plPhysicsI
{
    // setup/shutdown
    void (*initialize)(plPhysicsEngineSettings);
    void (*cleanup)(void);
    void (*reset)(void);

    // setttings
    void                    (*set_settings)(plPhysicsEngineSettings);
    plPhysicsEngineSettings (*get_settings)(void);

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

#endif // PL_PHYSICS_EXT_H