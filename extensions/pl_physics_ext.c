/*
   pl_physics_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] enums
// [SECTION] structs
// [SECTION] global context
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] intersection tests
// [SECTION] collision detection
// [SECTION] contact
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h>
#include <stdbool.h>
#include "pl.h"
#include "pl_physics_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"

// extensions
#include "pl_draw_ext.h"

// unstable extensions
#include "pl_ecs_ext.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    static const plEcsI*     gptECS     = NULL;
    static const plDrawI*    gptDraw    = NULL;
    static const plProfileI* gptProfile = NULL;
    static const plLogI*     gptLog     = NULL;
    static const plStatsI*   gptStats   = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plPhysicsContext     plPhysicsContext;
typedef struct _plRigidBody          plRigidBody;
typedef struct _plContact            plContact;
typedef struct _plContactResolver    plContactResolver;
typedef struct _plCollisionPrimitive plCollisionPrimitive;

// enums
typedef int plCollisionPrimitiveType;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plCollisionPrimitiveType
{
    PL_COLLISION_PRIMITIVE_TYPE_BOX,
    PL_COLLISION_PRIMITIVE_TYPE_SPHERE,
    PL_COLLISION_PRIMITIVE_TYPE_PLANE
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plRigidBody
{
    plEntity tEntity;
    plVec3   tPosition;
    plVec4   tOrientation;
    plMat4   tTransform;
    plMat3   tInverseInertiaTensor;
    plMat3   tInverseInertiaTensorWorld;
    plVec3   tLinearVelocity;
    plVec3   tAnglularVelocity;
    plVec3   tLastFrameAcceleration;
    plVec3   tAcceleration;
    float    fLinearDamping;
    float    fAngularDamping;
    float    fInverseMass;
    float    fMotion;
    bool     bIsAwake;
    bool     bCanSleep;
    plVec3   tForceAccumulation;
    plVec3   tTorqueAccumulation;
} plRigidBody;

typedef struct _plContact
{
    uint32_t atBodyIndices[2];
    float    fFriction;
    float    fRestitution;
    plVec3   tContactPoint;
    plVec3   tContactNormal;
    float    fPenetration;
    plMat3   tContactToWorld;
    plVec3   tContactVelocity;
    float    fDesiredDeltaVelocity;
    plVec3   atRelativeContactPositions[2];
} plContact;

typedef struct _plCollisionPrimitive
{
    plCollisionPrimitiveType tType;
    uint32_t uBodyIndex;
    plMat4  tTransform;
    float    fFriction;
  
    // box
    plVec3 tHalfSize;
    
    // sphere
    float fRadius;

    // plane
    plVec3 tDirection;
    float  fOffset;
} plCollisionPrimitive;

typedef struct _plPhysicsContext
{
    // admin data
    uint64_t                uLogChannel;
    plPhysicsEngineSettings tSettings;

    // system data
    plRigidBody* sbtRigidBodies;

    // collision data
    plCollisionPrimitive* sbtPrimitives;
    plContact*            sbtContactArray;
    float                 fRestitution;
    // float                 fTolerance; // close items should do contact
} plPhysicsContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plPhysicsContext* gptPhysicsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline void
pl__transform_inertia_tensor(plMat3* iitWorld, const plVec4* q, const plMat3* iitBody, const plMat4* rotmat)
{

    float t4  = rotmat->d[0] * iitBody->d[0] + rotmat->d[4] * iitBody->d[1] +  rotmat->d[8] * iitBody->d[2];
    float t9  = rotmat->d[0] * iitBody->d[3] + rotmat->d[4] * iitBody->d[4] +  rotmat->d[8] * iitBody->d[5];
    float t14 = rotmat->d[0] * iitBody->d[6] + rotmat->d[4] * iitBody->d[7] +  rotmat->d[8] * iitBody->d[8];
    float t28 = rotmat->d[1] * iitBody->d[0] + rotmat->d[5] * iitBody->d[1] +  rotmat->d[9] * iitBody->d[2];
    float t33 = rotmat->d[1] * iitBody->d[3] + rotmat->d[5] * iitBody->d[4] +  rotmat->d[9] * iitBody->d[5];
    float t38 = rotmat->d[1] * iitBody->d[6] + rotmat->d[5] * iitBody->d[7] +  rotmat->d[9] * iitBody->d[8];
    float t52 = rotmat->d[2] * iitBody->d[0] + rotmat->d[6] * iitBody->d[1] + rotmat->d[10] * iitBody->d[2];
    float t57 = rotmat->d[2] * iitBody->d[3] + rotmat->d[6] * iitBody->d[4] + rotmat->d[10] * iitBody->d[5];
    float t62 = rotmat->d[2] * iitBody->d[6] + rotmat->d[6] * iitBody->d[7] + rotmat->d[10] * iitBody->d[8];

    iitWorld->d[0] =  t4 * rotmat->d[0] +  t9 * rotmat->d[4] + t14 * rotmat->d[8];
    iitWorld->d[3] =  t4 * rotmat->d[1] +  t9 * rotmat->d[5] + t14 * rotmat->d[9];
    iitWorld->d[6] =  t4 * rotmat->d[2] +  t9 * rotmat->d[6] + t14 * rotmat->d[10];
    iitWorld->d[1] = t28 * rotmat->d[0] + t33 * rotmat->d[4] + t38 * rotmat->d[8];
    iitWorld->d[4] = t28 * rotmat->d[1] + t33 * rotmat->d[5] + t38 * rotmat->d[9];
    iitWorld->d[7] = t28 * rotmat->d[2] + t33 * rotmat->d[6] + t38 * rotmat->d[10];
    iitWorld->d[2] = t52 * rotmat->d[0] + t57 * rotmat->d[4] + t62 * rotmat->d[8];
    iitWorld->d[5] = t52 * rotmat->d[1] + t57 * rotmat->d[5] + t62 * rotmat->d[9];
    iitWorld->d[8] = t52 * rotmat->d[2] + t57 * rotmat->d[6] + t62 * rotmat->d[10];
}

static inline void
pl__set_awake(plRigidBody* ptBody, bool bAwake)
{
    if(bAwake)
    {
        ptBody->bIsAwake = true;
        ptBody->fMotion = gptPhysicsCtx->tSettings.fSleepEpsilon * 5.0f;
    }
    else
    {
        ptBody->bIsAwake = false;
        ptBody->tLinearVelocity = (plVec3){0};
        ptBody->tAnglularVelocity = (plVec3){0};
    }
}

// intersection tests
static bool pl__intersect_box_half_space   (const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptPlane);
static bool pl__intersect_box_box          (const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1);
static bool pl__intersect_sphere_half_space(const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane);
static bool pl__intersect_sphere_sphere    (const plCollisionPrimitive* ptSphere0, const plCollisionPrimitive* ptSphere1);

// collision detection
static void pl__collision_sphere_half_space(const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane);
static void pl__collision_sphere_true_plane(const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane);
static void pl__collision_sphere_sphere    (const plCollisionPrimitive* ptSphere0, const plCollisionPrimitive* ptSphere1);
static void pl__collision_box_half_space   (const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptPlane);
static void pl__collision_box_box          (const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1);
static void pl__collision_box_point        (const plCollisionPrimitive* ptBox, plVec3 tPoint);
static void pl__collision_box_sphere       (const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptSphere);

// contact
static void   pl__contact_calculate_internals             (plContact*, float fDeltaTime);
static void   pl__contact_calculate_desired_delta_velocity(plContact*, float fDeltaTime);
static plVec3 pl__contact_calculate_local_velocity        (plContact*, uint32_t uBodyIndex, float fDeltaTime);
static void   pl__contact_calculate_contact_basis         (plContact*);
static void   pl__contact_apply_velocity_change           (plContact*, plVec3 atVelocityChange[2], plVec3 atRotationChange[2]);
static void   pl__contact_apply_position_change           (plContact*, plVec3 atLinearChange[2], plVec3 atAngularChange[2], float fPenetration);
static plVec3 pl__contact_calculate_frictionless_impulse  (plContact*, plMat3* ptInverseInertiaTensor);
static plVec3 pl__contact_calculate_friction_impulse      (plContact*, plMat3* ptInverseInertiaTensor);

// contact resolver
static void  pl__contact_adjust_positions(float fDeltaTime);
static void  pl__contact_adjust_velocities(float fDeltaTime);
static void  pl__resolve_contacts            (float fDeltaTime);

// misc
static bool  pl__overlap_on_axis(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1, plVec3 tAxis, plVec3 tToCenter);
static float pl__transform_to_axis(const plCollisionPrimitive* ptBox, const plVec3* ptAxis);
static void  pl__physics_integrate(float fDeltaTime, plRigidBody* atBodies, uint32_t uBodyCount);
static void  pl_physics_draw(plComponentLibrary* ptLibrary, plDrawList3D* ptDrawlist);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_physics_set_settings(plPhysicsEngineSettings tSettings)
{

    if(tSettings.fSleepEpsilon == 0.0f)
        tSettings.fSleepEpsilon = 0.5f;

    if(tSettings.fPositionEpsilon == 0.0f)
        tSettings.fPositionEpsilon = 0.01f;

    if(tSettings.fVelocityEpsilon == 0.0f)
        tSettings.fVelocityEpsilon = 0.01f;

    if(tSettings.uMaxPositionIterations == 0)
        tSettings.uMaxPositionIterations = 256;

    if(tSettings.uMaxVelocityIterations == 0)
        tSettings.uMaxVelocityIterations = 256;

    gptPhysicsCtx->tSettings = tSettings;
}

plPhysicsEngineSettings
pl_physics_get_settings(void)
{
    return gptPhysicsCtx->tSettings;
}

void
pl_physics_initialize(plPhysicsEngineSettings tSettings)
{

    pl_physics_set_settings(tSettings);

    gptPhysicsCtx->fRestitution = 0.2f;

    plLogExtChannelInit tLogInit = {
        .tType       = PL_LOG_CHANNEL_TYPE_CYCLIC_BUFFER,
        .uEntryCount = 1024
    };
    gptPhysicsCtx->uLogChannel = gptLog->add_channel("Physics", tLogInit);

    pl_log_info(gptLog, gptPhysicsCtx->uLogChannel, "Physics initialized");
}

void
pl_physics_cleanup(void)
{
    pl_sb_free(gptPhysicsCtx->sbtRigidBodies);
    pl_sb_free(gptPhysicsCtx->sbtContactArray);
    pl_sb_free(gptPhysicsCtx->sbtPrimitives);
}

static void
pl__physics_update_force_fields(float fDeltaTime, plComponentLibrary* ptLibrary)
{
    plEntity* sbtRigidBodyEntities = ptLibrary->tRigidBodyPhysicsComponentManager.sbtEntities;
    const uint32_t uRigidBodyCount = pl_sb_size(sbtRigidBodyEntities);

    plEntity* sbtForceFieldEntities = ptLibrary->tForceFieldComponentManager.sbtEntities;
    const uint32_t uForceFieldCount = pl_sb_size(sbtForceFieldEntities);
    for(uint32_t i = 0; i < uForceFieldCount; i++)
    {
        plEntity tEntity = sbtForceFieldEntities[i];
        plForceFieldComponent* ptForceField = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_FORCE_FIELD, tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);

        if(ptForceField->tType == PL_FORCE_FIELD_TYPE_POINT)
        {
            for(uint32_t j = 0; j < uRigidBodyCount; j++)
            {
                plRigidBody* ptParticle = &gptPhysicsCtx->sbtRigidBodies[j];

                plVec3 tDirection = pl_sub_vec3(ptParticle->tPosition, ptTransform->tTranslation);

                float fDistance2 = pl_length_sqr_vec3(tDirection);

                if(fDistance2 < ptForceField->fRange * ptForceField->fRange)
                {
                    tDirection = pl_norm_vec3(tDirection);
                    ptParticle->tForceAccumulation = pl_add_vec3(ptParticle->tForceAccumulation, pl_mul_vec3_scalarf(tDirection, -ptForceField->fGravity / ptParticle->fInverseMass));
                    pl__set_awake(ptParticle, true);
                }
            }
        }

        else if(ptForceField->tType == PL_FORCE_FIELD_TYPE_PLANE)
        {
            plVec3 tForceDirection = {0.0f, 0.0f, 1.0f};
            tForceDirection = pl_mul_quat_vec3(tForceDirection, ptTransform->tRotation);
            tForceDirection = pl_norm_vec3(tForceDirection);

            for(uint32_t j = 0; j < uRigidBodyCount; j++)
            {
                plRigidBody* ptParticle = &gptPhysicsCtx->sbtRigidBodies[j];

                float fDistance = tForceDirection.x * (ptParticle->tPosition.x - ptTransform->tTranslation.x) +
                tForceDirection.y * (ptParticle->tPosition.y - ptTransform->tTranslation.y) +
                tForceDirection.z * (ptParticle->tPosition.z - ptTransform->tTranslation.z);

                if(fabsf(fDistance) < ptForceField->fRange)
                {
                    if(fDistance < 0)
                        ptParticle->tForceAccumulation = pl_add_vec3(ptParticle->tForceAccumulation, pl_mul_vec3_scalarf(tForceDirection, ptForceField->fGravity / ptParticle->fInverseMass));
                    else
                        ptParticle->tForceAccumulation = pl_add_vec3(ptParticle->tForceAccumulation, pl_mul_vec3_scalarf(tForceDirection, -ptForceField->fGravity / ptParticle->fInverseMass));
                    pl__set_awake(ptParticle, true);
                }
            }
        }
    }
}

void
pl_physics_update(float fDeltaTime, plComponentLibrary* ptLibrary)
{

    if(!gptPhysicsCtx->tSettings.bEnabled)
        return;

    pl_begin_cpu_sample(gptProfile, 0, "Physics Update");

    plEntity* sbtRigidBodyEntities = ptLibrary->tRigidBodyPhysicsComponentManager.sbtEntities;
    const uint32_t uRigidBodyCount = pl_sb_size(sbtRigidBodyEntities);
    
    pl_begin_cpu_sample(gptProfile, 0, "Update Objects");
    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plEntity tEntity = sbtRigidBodyEntities[i];
        plRigidBodyPhysicsComponent* ptRigidBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);
        if(ptRigidBody->uPhysicsObject == UINT64_MAX)
        {
            ptRigidBody->uPhysicsObject = pl_sb_size(gptPhysicsCtx->sbtRigidBodies);

            plVec3 tPosition = pl_add_vec3(ptTransform->tTranslation, ptRigidBody->tLocalOffset);

            plMat4 tTransform = pl_rotation_translation_scale(ptTransform->tRotation, tPosition, (plVec3){1.0f, 1.0f, 1.0f});

            plRigidBody tBody = {
                .fLinearDamping    = 1.0f - ptRigidBody->fLinearDamping,
                .fAngularDamping   = 1.0f - ptRigidBody->fAngularDamping,
                .fInverseMass      = 1.0f / ptRigidBody->fMass,
                .tPosition         = tPosition,
                .tOrientation      = ptTransform->tRotation,
                .tAcceleration     = ptRigidBody->tGravity,
                .tTransform        = tTransform,
                .tLinearVelocity   = (plVec3){0},
                .tAnglularVelocity = (plVec3){0},
                .tEntity           = tEntity,
                .bIsAwake          = true,
                .bCanSleep         = !(ptRigidBody->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING)
            };

            if(ptRigidBody->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_START_SLEEPING)
                pl__set_awake(&tBody, false);
            else
                pl__set_awake(&tBody, true);

            if(ptRigidBody->tShape == PL_COLLISION_SHAPE_BOX)
            {

                plVec3 tSquares = pl_mul_vec3_scalarf(ptRigidBody->tExtents, 0.5f);
                tSquares = pl_mul_vec3(tSquares, tSquares);

                float ix = 0.3f * ptRigidBody->fMass * (tSquares.y + tSquares.z);
                float iy = 0.3f * ptRigidBody->fMass * (tSquares.x + tSquares.z);
                float iz = 0.3f * ptRigidBody->fMass * (tSquares.x + tSquares.y);

                plMat3 tInertiaTensor = {0};

                tInertiaTensor.col[0].x  = ix;
                tInertiaTensor.col[1].y  = iy;
                tInertiaTensor.col[2].z  = iz;
                tBody.tInverseInertiaTensor = pl_mat3_invert(&tInertiaTensor);
            }
            else if(ptRigidBody->tShape == PL_COLLISION_SHAPE_SPHERE)
            {

                float ix = (2.0f / 5.0f) * ptRigidBody->fMass * ptRigidBody->fRadius * ptRigidBody->fRadius;
                plMat3 tInertiaTensor = {0};

                tInertiaTensor.col[0].x  = ix;
                tInertiaTensor.col[1].y  = ix;
                tInertiaTensor.col[2].z  = ix;
                tBody.tInverseInertiaTensor = pl_mat3_invert(&tInertiaTensor);
            }

            pl__transform_inertia_tensor(&tBody.tInverseInertiaTensorWorld, &tBody.tOrientation, &tBody.tInverseInertiaTensor, &tBody.tTransform);

            pl_sb_push(gptPhysicsCtx->sbtRigidBodies, tBody);
        }
        else
        {
            plVec3 tPosition = pl_add_vec3(ptTransform->tTranslation, ptRigidBody->tLocalOffset);
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tTransform = pl_rotation_translation_scale(ptTransform->tRotation, tPosition, (plVec3){1.0f, 1.0f, 1.0f});

            if(ptTransform->tFlags & PL_TRANSFORM_FLAGS_DIRTY)
                pl__set_awake(&gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject], true);
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].fLinearDamping = 1.0f - ptRigidBody->fLinearDamping;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].fAngularDamping = 1.0f - ptRigidBody->fAngularDamping;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].fInverseMass = 1.0f / ptRigidBody->fMass;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tPosition = tPosition;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tOrientation = ptTransform->tRotation;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tAcceleration = ptRigidBody->tGravity;

            if(ptRigidBody->tShape == PL_COLLISION_SHAPE_BOX)
            {

                plVec3 tSquares = pl_mul_vec3_scalarf(ptRigidBody->tExtents, 0.5f);
                tSquares = pl_mul_vec3(tSquares, tSquares);

                float ix = 0.3f * ptRigidBody->fMass * (tSquares.y + tSquares.z);
                float iy = 0.3f * ptRigidBody->fMass * (tSquares.x + tSquares.z);
                float iz = 0.3f * ptRigidBody->fMass * (tSquares.x + tSquares.y);

                plMat3 tInertiaTensor = {0};

                tInertiaTensor.col[0].x  = ix;
                tInertiaTensor.col[1].y  = iy;
                tInertiaTensor.col[2].z  = iz;
                gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tInverseInertiaTensor = pl_mat3_invert(&tInertiaTensor);
            }
            else if(ptRigidBody->tShape == PL_COLLISION_SHAPE_SPHERE)
            {

                float ix = (2.0f / 5.0f) * ptRigidBody->fMass * ptRigidBody->fRadius * ptRigidBody->fRadius;
                plMat3 tInertiaTensor = {0};

                tInertiaTensor.col[0].x  = ix;
                tInertiaTensor.col[1].y  = ix;
                tInertiaTensor.col[2].z  = ix;
                gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tInverseInertiaTensor = pl_mat3_invert(&tInertiaTensor);
            }

            pl__transform_inertia_tensor(&gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tInverseInertiaTensorWorld, &ptTransform->tRotation, &gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tInverseInertiaTensor, &gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tTransform);

        }
    }
    pl_end_cpu_sample(gptProfile, 0);

    // update stats
    static double* pdPhysicsObjects = NULL;
    if(!pdPhysicsObjects)
        pdPhysicsObjects = gptStats->get_counter("physics objects");
    *pdPhysicsObjects = (double)uRigidBodyCount;

    pl_begin_cpu_sample(gptProfile, 0, "Update Force Fields");
    pl__physics_update_force_fields(fDeltaTime, ptLibrary);
    pl_end_cpu_sample(gptProfile, 0);

    plCollisionPrimitive tPrimFloor = {
        .tType = PL_COLLISION_PRIMITIVE_TYPE_PLANE,
        .uBodyIndex = UINT32_MAX,
        .tTransform = pl_identity_mat4(),
        .tDirection = {0.0f, 1.0f, 0.0f},
        .fFriction = 0.9f
    };

    pl_sb_reset(gptPhysicsCtx->sbtContactArray);
    pl_sb_reset(gptPhysicsCtx->sbtPrimitives);

    pl_begin_cpu_sample(gptProfile, 0, "Collision Detection");
    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plEntity tEntity = sbtRigidBodyEntities[i];
        plRigidBodyPhysicsComponent* ptRigidBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);

        if(ptRigidBody->tShape == PL_COLLISION_SHAPE_BOX)
        {
            plCollisionPrimitive tPrim = {
                .tType      = PL_COLLISION_PRIMITIVE_TYPE_BOX,
                .uBodyIndex = i,
                .tTransform = gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tTransform,
                .tHalfSize  = pl_mul_vec3_scalarf(ptRigidBody->tExtents, 0.5f),
                .fFriction  = ptRigidBody->fFriction
            };
            pl_sb_push(gptPhysicsCtx->sbtPrimitives, tPrim);
            pl__collision_box_half_space(&tPrim, &tPrimFloor);
        }
        else if(ptRigidBody->tShape == PL_COLLISION_SHAPE_SPHERE)
        {
            plCollisionPrimitive tPrim = {
                .tType      = PL_COLLISION_PRIMITIVE_TYPE_SPHERE,
                .uBodyIndex = i,
                .tTransform = gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tTransform,
                .fRadius    = ptRigidBody->fRadius,
                .fFriction  = ptRigidBody->fFriction
            };
            pl_sb_push(gptPhysicsCtx->sbtPrimitives, tPrim);
            pl__collision_sphere_half_space(&tPrim, &tPrimFloor);
        }
        
    }

    for(uint32_t i = 0; i < uRigidBodyCount - 1; i++)
    {
        
        for(uint32_t j = i + 1; j < uRigidBodyCount; j++)
        {
            if(gptPhysicsCtx->sbtRigidBodies[gptPhysicsCtx->sbtPrimitives[i].uBodyIndex].bIsAwake || gptPhysicsCtx->sbtRigidBodies[gptPhysicsCtx->sbtPrimitives[j].uBodyIndex].bIsAwake)
            {
                if(gptPhysicsCtx->sbtPrimitives[i].tType == PL_COLLISION_PRIMITIVE_TYPE_BOX && gptPhysicsCtx->sbtPrimitives[j].tType == PL_COLLISION_PRIMITIVE_TYPE_BOX)
                    pl__collision_box_box(&gptPhysicsCtx->sbtPrimitives[i], &gptPhysicsCtx->sbtPrimitives[j]);
                else if(gptPhysicsCtx->sbtPrimitives[i].tType == PL_COLLISION_PRIMITIVE_TYPE_BOX && gptPhysicsCtx->sbtPrimitives[j].tType == PL_COLLISION_PRIMITIVE_TYPE_SPHERE)
                    pl__collision_box_sphere(&gptPhysicsCtx->sbtPrimitives[i], &gptPhysicsCtx->sbtPrimitives[j]);
                else if(gptPhysicsCtx->sbtPrimitives[i].tType == PL_COLLISION_PRIMITIVE_TYPE_SPHERE && gptPhysicsCtx->sbtPrimitives[j].tType == PL_COLLISION_PRIMITIVE_TYPE_BOX)
                    pl__collision_box_sphere(&gptPhysicsCtx->sbtPrimitives[j], &gptPhysicsCtx->sbtPrimitives[i]);
                else if(gptPhysicsCtx->sbtPrimitives[i].tType == PL_COLLISION_PRIMITIVE_TYPE_SPHERE && gptPhysicsCtx->sbtPrimitives[j].tType == PL_COLLISION_PRIMITIVE_TYPE_SPHERE)
                    pl__collision_sphere_sphere(&gptPhysicsCtx->sbtPrimitives[i], &gptPhysicsCtx->sbtPrimitives[j]);
            }
        }
    }
    pl_end_cpu_sample(gptProfile, 0);

    pl_begin_cpu_sample(gptProfile, 0, "Resolve Contact");
    pl__resolve_contacts(fDeltaTime);
    pl_end_cpu_sample(gptProfile, 0);

    pl_begin_cpu_sample(gptProfile, 0, "Integration");
    pl__physics_integrate(fDeltaTime, gptPhysicsCtx->sbtRigidBodies, uRigidBodyCount);
    pl_end_cpu_sample(gptProfile, 0);

    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plTransformComponent* ptSphereTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, gptPhysicsCtx->sbtRigidBodies[i].tEntity);
        plRigidBodyPhysicsComponent* ptRigidBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, gptPhysicsCtx->sbtRigidBodies[i].tEntity);
        ptSphereTransform->tRotation = gptPhysicsCtx->sbtRigidBodies[i].tOrientation;
        ptSphereTransform->tTranslation = pl_sub_vec3(gptPhysicsCtx->sbtRigidBodies[i].tPosition, ptRigidBody->tLocalOffset);
        ptSphereTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
    }

    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_physics_reset(void)
{
    pl_sb_reset(gptPhysicsCtx->sbtRigidBodies);
}

void
pl_physics_draw(plComponentLibrary* ptLibrary, plDrawList3D* ptDrawlist)
{
    pl_begin_cpu_sample(gptProfile, 0, "Physics Draw");
    plEntity* sbtRigidBodyEntities = ptLibrary->tRigidBodyPhysicsComponentManager.sbtEntities;
    const uint32_t uRigidBodyCount = pl_sb_size(sbtRigidBodyEntities);
    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plEntity tEntity = sbtRigidBodyEntities[i];
        plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);

        uint32_t uColor = PL_COLOR_32_CYAN;
        if(ptBody->uPhysicsObject != UINT64_MAX)
            uColor = gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject].bIsAwake ? PL_COLOR_32_GREEN : PL_COLOR_32_YELLOW;

        if(ptBody->tShape == PL_COLLISION_SHAPE_SPHERE)
        {
            plDrawSphereDesc tDesc = {
                .fRadius = ptBody->fRadius,
                .tCenter = pl_add_vec3(ptTransform->tTranslation, ptBody->tLocalOffset)
            };
            gptDraw->add_3d_sphere(ptDrawlist, tDesc, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.01f});
        }
        else if(ptBody->tShape == PL_COLLISION_SHAPE_BOX)
        {
            plVec3 tScale = {0};
            plVec4 tRotation = {0};
            plVec3 tTranslation = {0};
            pl_decompose_matrix(&ptTransform->tWorld, &tScale, &tRotation, &tTranslation);

            tTranslation = pl_add_vec3(tTranslation, ptBody->tLocalOffset);

            plMat4 tTransform = pl_rotation_translation_scale(tRotation, tTranslation, (plVec3){1.0f, 1.0f, 1.0f});

            plVec3 tP0 = { 0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};
            plVec3 tP1 = { 0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y, -0.5f * ptBody->tExtents.z};
            plVec3 tP2 = {-0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y, -0.5f * ptBody->tExtents.z};
            plVec3 tP3 = {-0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};
            plVec3 tP4 = {  0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};
            plVec3 tP5 = {  0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  -0.5f * ptBody->tExtents.z};
            plVec3 tP6 = { -0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  -0.5f * ptBody->tExtents.z};
            plVec3 tP7 = { -0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};

            tP0 = pl_mul_mat4_vec3(&tTransform, tP0);
            tP1 = pl_mul_mat4_vec3(&tTransform, tP1);
            tP2 = pl_mul_mat4_vec3(&tTransform, tP2);
            tP3 = pl_mul_mat4_vec3(&tTransform, tP3);
            tP4 = pl_mul_mat4_vec3(&tTransform, tP4);
            tP5 = pl_mul_mat4_vec3(&tTransform, tP5);
            tP6 = pl_mul_mat4_vec3(&tTransform, tP6);
            tP7 = pl_mul_mat4_vec3(&tTransform, tP7);


            gptDraw->add_3d_line(ptDrawlist, tP0, tP1, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP1, tP2, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP2, tP3, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP3, tP0, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});

            gptDraw->add_3d_line(ptDrawlist, tP4, tP5, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP5, tP6, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP6, tP7, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP7, tP4, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});

            gptDraw->add_3d_line(ptDrawlist, tP0, tP4, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP1, tP5, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP2, tP6, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP3, tP7, (plDrawLineOptions){.uColor = uColor, .fThickness = 0.05f});
        }
    }

    plEntity* sbtForceFieldEntities = ptLibrary->tForceFieldComponentManager.sbtEntities;
    const uint32_t uForceFieldCount = pl_sb_size(sbtForceFieldEntities);
    for(uint32_t i = 0; i < uForceFieldCount; i++)
    {
        plEntity tEntity = sbtForceFieldEntities[i];
        plForceFieldComponent* ptForceField = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_FORCE_FIELD, tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);

        if(ptForceField->tType == PL_FORCE_FIELD_TYPE_POINT)
        {
            gptDraw->add_3d_cross(ptDrawlist, ptTransform->tTranslation, 0.25f, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});

            plDrawSphereDesc tDesc = {
                .fRadius = ptForceField->fRange,
                .tCenter = ptTransform->tTranslation
            };
            gptDraw->add_3d_sphere(ptDrawlist, tDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.01f});
        }
        else if(ptForceField->tType == PL_FORCE_FIELD_TYPE_PLANE)
        {
            plVec3 tDirection = {0.0f, 0.0f, 1.0f};
            tDirection = pl_mul_quat_vec3(tDirection, ptTransform->tRotation);

            gptDraw->add_3d_cross(ptDrawlist, ptTransform->tTranslation, 0.25f, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});

            gptDraw->add_3d_line(ptDrawlist, ptTransform->tTranslation, pl_add_vec3(ptTransform->tTranslation, tDirection), (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
        }
    }
    pl_end_cpu_sample(gptProfile, 0);
}

void
pl_physics_apply_force(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tForce)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

        ptRigidBody->tForceAccumulation = pl_add_vec3(ptRigidBody->tForceAccumulation, tForce);
        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_apply_force_at_point(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tForce, plVec3 tPoint)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

        ptRigidBody->tForceAccumulation = pl_add_vec3(ptRigidBody->tForceAccumulation, tForce);

        tPoint = pl_sub_vec3(tPoint, ptRigidBody->tPosition);

        plVec3 tTorque = {
            tPoint.y * tForce.z - tPoint.z * tForce.y,
            tPoint.z * tForce.x - tPoint.x * tForce.z,
            tPoint.x * tForce.y - tPoint.y * tForce.x
        };
    
        ptRigidBody->tTorqueAccumulation = pl_add_vec3(ptRigidBody->tTorqueAccumulation, tTorque);

        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_apply_force_at_body_point(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tForce, plVec3 tPoint)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

        ptRigidBody->tForceAccumulation = pl_add_vec3(ptRigidBody->tForceAccumulation, tForce);

        tPoint = pl_mul_mat4_vec4(&ptRigidBody->tTransform, (plVec4){.xyz = tPoint, .w = 1.0f}).xyz;

        tPoint = pl_sub_vec3(tPoint, ptRigidBody->tPosition);

        plVec3 tTorque = {
            tPoint.y * tForce.z - tPoint.z * tForce.y,
            tPoint.z * tForce.x - tPoint.x * tForce.z,
            tPoint.x * tForce.y - tPoint.y * tForce.x
        };
    
        ptRigidBody->tTorqueAccumulation = pl_add_vec3(ptRigidBody->tTorqueAccumulation, tTorque);

        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_apply_impulse(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tForce)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

        ptRigidBody->tLinearVelocity = pl_add_vec3(ptRigidBody->tLinearVelocity, pl_mul_vec3_scalarf(tForce, ptRigidBody->fInverseMass));

        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_apply_impulse_at_point(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tForce, plVec3 tPoint)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

        ptRigidBody->tLinearVelocity = pl_add_vec3(ptRigidBody->tLinearVelocity, pl_mul_vec3_scalarf(tForce, ptRigidBody->fInverseMass));

        tPoint = pl_sub_vec3(tPoint, ptRigidBody->tPosition);

        plVec3 tTorque = {
            tPoint.y * tForce.z - tPoint.z * tForce.y,
            tPoint.z * tForce.x - tPoint.x * tForce.z,
            tPoint.x * tForce.y - tPoint.y * tForce.x
        };

        plVec3 tAngularAcceleration = pl_mul_mat3_vec3(&ptRigidBody->tInverseInertiaTensorWorld, tTorque);
        ptRigidBody->tAnglularVelocity = pl_add_vec3(ptRigidBody->tAnglularVelocity, tAngularAcceleration);


        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_apply_impulse_at_body_point(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tForce, plVec3 tPoint)
{

    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

        ptRigidBody->tLinearVelocity = pl_add_vec3(ptRigidBody->tLinearVelocity, pl_mul_vec3_scalarf(tForce, ptRigidBody->fInverseMass));

        tPoint = pl_mul_mat4_vec4(&ptRigidBody->tTransform, (plVec4){.xyz = tPoint, .w = 1.0f}).xyz;
        tPoint = pl_sub_vec3(tPoint, ptRigidBody->tPosition);
        plVec3 tTorque = {
            tPoint.y * tForce.z - tPoint.z * tForce.y,
            tPoint.z * tForce.x - tPoint.x * tForce.z,
            tPoint.x * tForce.y - tPoint.y * tForce.x
        };

        plVec3 tAngularAcceleration = pl_mul_mat3_vec3(&ptRigidBody->tInverseInertiaTensorWorld, tTorque);
        ptRigidBody->tAnglularVelocity = pl_add_vec3(ptRigidBody->tAnglularVelocity, tAngularAcceleration);

        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_apply_torque(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tTorque)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

        ptRigidBody->tTorqueAccumulation = pl_add_vec3(ptRigidBody->tTorqueAccumulation, tTorque);
        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_wake_up_body(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];
        pl__set_awake(ptRigidBody, true);
    }
}

void
pl_physics_sleep_body(plComponentLibrary* ptLibrary, plEntity tEntity)
{
    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);

    if(ptBody)
    {
        plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];
        pl__set_awake(ptRigidBody, false);
    }
}

void
pl_physics_wake_up_all(void)
{
    const uint32_t uBodyCount = pl_sb_size(gptPhysicsCtx->sbtRigidBodies);
    for(uint32_t i = 0; i < uBodyCount; i++)
    {
        pl__set_awake(&gptPhysicsCtx->sbtRigidBodies[i], true);
    }
}

void
pl_physics_sleep_all(void)
{
    const uint32_t uBodyCount = pl_sb_size(gptPhysicsCtx->sbtRigidBodies);
    for(uint32_t i = 0; i < uBodyCount; i++)
    {
        pl__set_awake(&gptPhysicsCtx->sbtRigidBodies[i], false);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static float
pl__transform_to_axis(const plCollisionPrimitive* ptBox, const plVec3* ptAxis)
{
    return ptBox->tHalfSize.x * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[0].xyz)) + 
        ptBox->tHalfSize.y * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[1].xyz)) + 
        ptBox->tHalfSize.z * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[2].xyz));
}

static bool
pl__overlap_on_axis(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1, plVec3 tAxis, plVec3 tToCenter)
{
    // project the half-size of one onto axis
    float fOneProject = pl__transform_to_axis(ptBox0, &tAxis);
    float fTwoProject = pl__transform_to_axis(ptBox1, &tAxis);

    // project this onto the axis
    float fDistance = fabsf(pl_dot_vec3(tToCenter, tAxis));

    // check for overlap
    return (fDistance < fOneProject + fTwoProject);
}

static plVec3
pl__contact_calculate_frictionless_impulse(plContact* ptContact, plMat3* ptInverseInertiaTensor)
{
    plVec3 tImpulseContact = {0};

    // Build a vector that shows the change in velocity in
    // world space for a unit impulse in the direction of the contact
    // normal.
    plVec3 tDeltaVelWorld = pl_cross_vec3(ptContact->atRelativeContactPositions[0], ptContact->tContactNormal);
    tDeltaVelWorld = pl_mul_mat3_vec3(&ptInverseInertiaTensor[0], tDeltaVelWorld);
    tDeltaVelWorld = pl_cross_vec3(tDeltaVelWorld, ptContact->atRelativeContactPositions[0]);

    // Work out the change in velocity in contact coordiantes.
    float fDeltaVelocity = pl_dot_vec3(tDeltaVelWorld, ptContact->tContactNormal);

    // Add the linear component of velocity change
    fDeltaVelocity += gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[0]].fInverseMass;

    // Check if we need to the second body's data
    if (ptContact->atBodyIndices[1] != UINT32_MAX)
    {

        // Go through the same transformation sequence again
        tDeltaVelWorld = pl_cross_vec3(ptContact->atRelativeContactPositions[1], ptContact->tContactNormal);
        tDeltaVelWorld = pl_mul_mat3_vec3(&ptInverseInertiaTensor[1], tDeltaVelWorld);
        tDeltaVelWorld = pl_cross_vec3(tDeltaVelWorld, ptContact->atRelativeContactPositions[1]);


        // Add the change in velocity due to rotation
        fDeltaVelocity += pl_dot_vec3(tDeltaVelWorld, ptContact->tContactNormal);

        // Add the change in velocity due to linear motion
        fDeltaVelocity += gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[1]].fInverseMass;
    }

    // Calculate the required size of the impulse
    tImpulseContact.x = ptContact->fDesiredDeltaVelocity / fDeltaVelocity;
    tImpulseContact.y = 0;
    tImpulseContact.z = 0;
    return tImpulseContact;
}

static plVec3
pl__contact_calculate_friction_impulse(plContact* ptContact, plMat3* ptInverseInertiaTensor)
{
    plVec3 tImpulseContact = {0};

    plRigidBody* ptBody0 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[0]];
    plRigidBody* ptBody1 = ptContact->atBodyIndices[1] == UINT32_MAX ? NULL : &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[1]];

    float fInverseMass = ptBody0->fInverseMass;

    // The equivalent of a cross product in matrices is multiplication
    // by a skew symmetric matrix - we build the matrix for converting
    // between linear and angular quantities.
    plMat3 tImpulseToTorque = pl_identity_mat3();
    tImpulseToTorque.col[0].x = tImpulseToTorque.col[1].y = tImpulseToTorque.col[2].z = 0;
    tImpulseToTorque.col[1].x = -ptContact->atRelativeContactPositions[0].z;
    tImpulseToTorque.col[2].x = ptContact->atRelativeContactPositions[0].y;
    tImpulseToTorque.col[0].y = ptContact->atRelativeContactPositions[0].z;
    tImpulseToTorque.col[2].y = -ptContact->atRelativeContactPositions[0].x;
    tImpulseToTorque.col[0].z = -ptContact->atRelativeContactPositions[0].y;
    tImpulseToTorque.col[1].z = ptContact->atRelativeContactPositions[0].x;

    // Build the matrix to convert contact impulse to change in velocity
    // in world coordinates.
    plMat3 tDeltaVelWorld = tImpulseToTorque;
    tDeltaVelWorld = pl_mul_mat3(&tDeltaVelWorld, &ptInverseInertiaTensor[0]);
    tDeltaVelWorld = pl_mul_mat3(&tDeltaVelWorld, &tImpulseToTorque);
    tDeltaVelWorld = pl_mul_scalarf_mat3(-1.0f, &tDeltaVelWorld);

    // Check if we need to add body two's data
    if (ptBody1)
    {
        // Set the cross product matrix
        tImpulseToTorque.col[0].x = tImpulseToTorque.col[1].y = tImpulseToTorque.col[2].z = 0;
        tImpulseToTorque.col[1].x = -ptContact->atRelativeContactPositions[1].z;
        tImpulseToTorque.col[2].x = ptContact->atRelativeContactPositions[1].y;
        tImpulseToTorque.col[0].y = ptContact->atRelativeContactPositions[1].z;
        tImpulseToTorque.col[2].y = -ptContact->atRelativeContactPositions[1].x;
        tImpulseToTorque.col[0].z = -ptContact->atRelativeContactPositions[1].y;
        tImpulseToTorque.col[1].z = ptContact->atRelativeContactPositions[1].x;

        // Calculate the velocity change matrix
        plMat3 tDeltaVelWorld2 = tImpulseToTorque;
        tDeltaVelWorld2 = pl_mul_mat3(&tDeltaVelWorld2, &ptInverseInertiaTensor[1]);
        tDeltaVelWorld2 = pl_mul_mat3(&tDeltaVelWorld2, &tImpulseToTorque);
        tDeltaVelWorld2 = pl_mul_scalarf_mat3(-1.0f, &tDeltaVelWorld2);

        // Add to the total delta velocity.
        tDeltaVelWorld = pl_add_mat3(&tDeltaVelWorld, &tDeltaVelWorld2);

        // Add to the inverse mass
        fInverseMass += ptBody1->fInverseMass;
    }

    // Do a change of basis to convert into contact coordinates.
    plMat3 tDeltaVelocity = pl_mat3_invert(&ptContact->tContactToWorld);
    tDeltaVelocity = pl_mul_mat3(&tDeltaVelocity, &tDeltaVelWorld);
    tDeltaVelocity = pl_mul_mat3(&tDeltaVelocity, &ptContact->tContactToWorld);

    // Add in the linear velocity change
    tDeltaVelocity.col[0].x += fInverseMass; // d[0]
    tDeltaVelocity.col[1].y += fInverseMass; // d[4]
    tDeltaVelocity.col[2].z += fInverseMass; // d[8]

    // Invert to get the impulse needed per unit velocity
    plMat3 tImpulseMatrix = pl_mat3_invert(&tDeltaVelocity);

    // Find the target velocities to kill
    plVec3 tVelKill = {ptContact->fDesiredDeltaVelocity, -ptContact->tContactVelocity.y, -ptContact->tContactVelocity.z};

    // Find the impulse to kill target velocities
    tImpulseContact = pl_mul_mat3_vec3(&tImpulseMatrix, tVelKill);

    // Check for exceeding friction
    float fPlanarImpulse = sqrtf(tImpulseContact.y*tImpulseContact.y + tImpulseContact.z*tImpulseContact.z);
    if (fPlanarImpulse > tImpulseContact.x * ptContact->fFriction)
    {
        // We need to use dynamic friction
        tImpulseContact.y /= fPlanarImpulse;
        tImpulseContact.z /= fPlanarImpulse;

        tImpulseContact.x = tDeltaVelocity.col[0].x +
            tDeltaVelocity.col[1].x * ptContact->fFriction*tImpulseContact.y +
            tDeltaVelocity.col[2].x * ptContact->fFriction*tImpulseContact.z;
        tImpulseContact.x = ptContact->fDesiredDeltaVelocity / tImpulseContact.x;
        tImpulseContact.y *= ptContact->fFriction * tImpulseContact.x;
        tImpulseContact.z *= ptContact->fFriction * tImpulseContact.x;
    }
    return tImpulseContact;
}

static void
pl__contact_apply_velocity_change(plContact* ptContact, plVec3 atVelocityChange[2], plVec3 atRotationChange[2])
{
    // Get hold of the inverse mass and inverse inertia tensor, both in
    // world coordinates.
    plMat3 atInverseInertiaTensor[2] = {0};

    plRigidBody* ptBody0 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[0]];
    plRigidBody* ptBody1 = ptContact->atBodyIndices[1] == UINT32_MAX ? NULL : &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[1]];

    atInverseInertiaTensor[0] = ptBody0->tInverseInertiaTensorWorld;
    if(ptBody1)
        atInverseInertiaTensor[1] = ptBody1->tInverseInertiaTensorWorld;

    // We will calculate the impulse for each contact axis
    plVec3 tImpulseContact = {0};

    if (ptContact->fFriction == 0.0f)
    {
        // Use the short format for frictionless contacts
        tImpulseContact = pl__contact_calculate_frictionless_impulse(ptContact, atInverseInertiaTensor);
    }
    else
    {
        // Otherwise we may have impulses that aren't in the direction of the
        // contact, so we need the more complex version.
        tImpulseContact = pl__contact_calculate_friction_impulse(ptContact, atInverseInertiaTensor);
    }

    // Convert impulse to world coordinates
    plVec3 tImpulse = pl_mul_mat3_vec3(&ptContact->tContactToWorld, tImpulseContact);

    // Split in the impulse into linear and rotational components
    plVec3 tImpulsiveTorque = pl_cross_vec3(ptContact->atRelativeContactPositions[0], tImpulse);
    atRotationChange[0] = pl_mul_mat3_vec3(&atInverseInertiaTensor[0], tImpulsiveTorque);
    atVelocityChange[0] = pl_mul_vec3_scalarf(tImpulse, ptBody0->fInverseMass);

    // Apply the changes
    ptBody0->tLinearVelocity = pl_add_vec3(ptBody0->tLinearVelocity, atVelocityChange[0]);
    ptBody0->tAnglularVelocity = pl_add_vec3(ptBody0->tAnglularVelocity, atRotationChange[0]);

    if (ptBody1)
    {
        // Work out body one's linear and angular changes
        tImpulsiveTorque = pl_cross_vec3(tImpulse, ptContact->atRelativeContactPositions[1]);
        atRotationChange[1] = pl_mul_mat3_vec3(&atInverseInertiaTensor[1], tImpulsiveTorque);
        atVelocityChange[1] = pl_mul_vec3_scalarf(tImpulse, -ptBody1->fInverseMass);

        // And apply them.
        ptBody1->tLinearVelocity = pl_add_vec3(ptBody1->tLinearVelocity, atVelocityChange[1]);
        ptBody1->tAnglularVelocity = pl_add_vec3(ptBody1->tAnglularVelocity, atRotationChange[1]);
    }
}

static void
pl__contact_apply_position_change(plContact* ptContact, plVec3 atLinearChange[2], plVec3 atAngularChange[2], float fPenetration)
{
    const float fAngularLimit = 0.2f;
    float afAngularMove[2] = {0};
    float afLinearMove[2] = {0};

    float fTotalInertia = 0;
    float afLinearInertia[2] = {0};
    float afAngularInertia[2] = {0};

    // We need to work out the inertia of each object in the direction
    // of the contact normal, due to angular inertia only.
    for (uint32_t i = 0; i < 2; i++)
    {
        if(ptContact->atBodyIndices[i] == UINT32_MAX)
            continue;

        plRigidBody* ptBody = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[i]];

        plMat3 tInverseInertiaTensor = ptBody->tInverseInertiaTensorWorld;

        // Use the same procedure as for calculating frictionless
        // velocity change to work out the angular inertia.
        plVec3 tAngularInertiaWorld = pl_cross_vec3(ptContact->atRelativeContactPositions[i], ptContact->tContactNormal);
        tAngularInertiaWorld = pl_mul_mat3_vec3(&tInverseInertiaTensor, tAngularInertiaWorld);
        tAngularInertiaWorld = pl_cross_vec3(tAngularInertiaWorld, ptContact->atRelativeContactPositions[i]);
        afAngularInertia[i] = pl_dot_vec3(tAngularInertiaWorld, ptContact->tContactNormal);

        // linear component is simply inverse mass
        afLinearInertia[i] = ptBody->fInverseMass;

        // Keep track of the total inertia from all components
        fTotalInertia += afLinearInertia[i] + afAngularInertia[i];

        // We break the loop here so that the totalInertia value is
        // completely calculated (by both iterations) before
        // continuing.
    }

    // Loop through again calculating and applying the changes
    for (uint32_t i = 0; i < 2; i++)
    {
        if(ptContact->atBodyIndices[i] == UINT32_MAX)
            continue;

        plRigidBody* ptBody = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[i]];

        // The linear and angular movements required are in proportion to
        // the two inverse inertias.
        float fSign = (i == 0) ? 1.0f : -1.0f;
        afAngularMove[i] = fSign * ptContact->fPenetration * (afAngularInertia[i] / fTotalInertia);
        afLinearMove[i] = fSign * ptContact->fPenetration * (afLinearInertia[i] / fTotalInertia);

        // To avoid angular projections that are too great (when mass is large
        // but inertia tensor is small) limit the angular move.
        plVec3 tProjection = ptContact->atRelativeContactPositions[i];
        tProjection = pl_add_vec3(tProjection, pl_mul_vec3_scalarf(ptContact->tContactNormal, -pl_dot_vec3(ptContact->tContactNormal, ptContact->atRelativeContactPositions[i])));

        // Use the small angle approximation for the sine of the angle (i.e.
        // the magnitude would be sine(angularLimit) * projection.magnitude
        // but we approximate sine(angularLimit) to angularLimit).
        float fMaxMagnitude = fAngularLimit * pl_length_vec3(tProjection);

        if (afAngularMove[i] < -fMaxMagnitude)
        {
            float fTotalMove = afAngularMove[i] + afLinearMove[i];
            afAngularMove[i] = -fMaxMagnitude;
            afLinearMove[i] = fTotalMove - afAngularMove[i];
        }
        else if (afAngularMove[i] > fMaxMagnitude)
        {
            float fTotalMove = afAngularMove[i] + afLinearMove[i];
            afAngularMove[i] = fMaxMagnitude;
            afLinearMove[i] = fTotalMove - afAngularMove[i];
        }

        // We have the linear amount of movement required by turning
        // the rigid body (in angularMove[i]). We now need to
        // calculate the desired rotation to achieve that.
        if (afAngularMove[i] == 0)
        {
            // Easy case - no angular movement means no rotation.
            atAngularChange[i] = (plVec3){0};
        }
        else
        {
            // Work out the direction we'd like to rotate in.
            plVec3 tTargetAngularDirection = pl_cross_vec3(ptContact->atRelativeContactPositions[i], ptContact->tContactNormal);

            plMat3 tInverseInertiaTensor = ptBody->tInverseInertiaTensorWorld;

            // Work out the direction we'd need to rotate to achieve that
            atAngularChange[i] = pl_mul_vec3_scalarf(pl_mul_mat3_vec3(&tInverseInertiaTensor, tTargetAngularDirection), afAngularMove[i] / afAngularInertia[i]);
        }

        // Velocity change is easier - it is just the linear movement
        // along the contact normal.
        atLinearChange[i] = pl_mul_vec3_scalarf(ptContact->tContactNormal, afLinearMove[i]);

        // Now we can start to apply the values we've calculated.
        // Apply the linear movement
        plVec3 tPos = ptBody->tPosition;
        tPos = pl_add_vec3(tPos, pl_mul_vec3_scalarf(ptContact->tContactNormal, afLinearMove[i]));
        ptBody->tPosition = tPos;

        // And the change in orientation
        plVec4 tQ = ptBody->tOrientation;
        plVec4 tQ2 = {atAngularChange[i].x, atAngularChange[i].y, atAngularChange[i].z, 0.0f};
        // tQ2 = pl_mul_quat(tQ, tQ);
        tQ.x += tQ2.x * 0.5f;
        tQ.y += tQ2.y * 0.5f;
        tQ.z += tQ2.z * 0.5f;
        tQ.w += tQ2.w * 0.5f;
        ptBody->tOrientation = tQ;

        // We need to calculate the derived data for any body that is
        // asleep, so that the changes are reflected in the object's
        // data. Otherwise the resolution will not change the position
        // of the object, and the next collision detection round will
        // have the same penetration.
        if(!ptBody->bIsAwake)
        {
            // calculateDerivedData()
            ptBody->tOrientation = pl_norm_quat(ptBody->tOrientation);

            ptBody->tTransform = pl_rotation_translation_scale(ptBody->tOrientation, ptBody->tPosition, (plVec3){1.0f, 1.0f, 1.0f});

            pl__transform_inertia_tensor(&ptBody->tInverseInertiaTensorWorld, &ptBody->tOrientation, &ptBody->tInverseInertiaTensor, &ptBody->tTransform);
        }
    }
}

static inline void
pl__contact_match_awake_state(plContact* ptContact)
{
    plRigidBody* ptBody0 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[0]];
    plRigidBody* ptBody1 = ptContact->atBodyIndices[1] == UINT32_MAX ? NULL : &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[1]];

    if(ptBody1 == NULL)
        return;

    bool bBody0Awake = ptBody0->bIsAwake;
    bool bBody1Awake = ptBody1->bIsAwake;
    
    // wake up only sleeping one
    if(bBody0Awake ^ bBody1Awake)
    {
        if(bBody0Awake)
            pl__set_awake(ptBody1, true);
        else
            pl__set_awake(ptBody0, true);
    }
}

static void
pl__contact_adjust_positions(float fDeltaTime)
{
    const uint32_t uContactCount = pl_sb_size(gptPhysicsCtx->sbtContactArray);

    plVec3 atLinearChange[2] = {0};
    plVec3 atAngularChange[2] = {0};
    plVec3 tDeltaPosition = {0};

    uint32_t uPositionIterationsUsed = 0;

    float fMax = 0.0f;
    uint32_t uIndex = 0;
    while(uPositionIterationsUsed < gptPhysicsCtx->tSettings.uMaxPositionIterations)
    {
        // find biggest penetration
        fMax = gptPhysicsCtx->tSettings.fPositionEpsilon;
        uIndex = uContactCount;
        for(uint32_t i = 0; i < uContactCount; i++)
        {
            if(gptPhysicsCtx->sbtContactArray[i].fPenetration > fMax)
            {
                fMax = gptPhysicsCtx->sbtContactArray[i].fPenetration;
                uIndex = i;
            }
        }

        if(uIndex == uContactCount)
            break;
        
        // match awake state at contact
        pl__contact_match_awake_state(&gptPhysicsCtx->sbtContactArray[uIndex]);

        // resolve penetration
        pl__contact_apply_position_change(&gptPhysicsCtx->sbtContactArray[uIndex], atLinearChange, atAngularChange, fMax);

        // Again this action may have changed the penetration of other
        // bodies, so we update contacts.
        for (uint32_t i = 0; i < uContactCount; i++)
        {
            // Check each body in the contact
            for (uint32_t b = 0; b < 2; b++)
            {

                if(gptPhysicsCtx->sbtContactArray[i].atBodyIndices[b] == UINT32_MAX)
                    continue;

                plRigidBody* ptBody = &gptPhysicsCtx->sbtRigidBodies[gptPhysicsCtx->sbtContactArray[i].atBodyIndices[b]];

                // Check for a match with each body in the newly
                // resolved contact
                for (uint32_t d = 0; d < 2; d++)
                {
                    if (gptPhysicsCtx->sbtContactArray[i].atBodyIndices[b] == gptPhysicsCtx->sbtContactArray[uIndex].atBodyIndices[d])
                    {
                        tDeltaPosition = pl_cross_vec3(atAngularChange[d], gptPhysicsCtx->sbtContactArray[i].atRelativeContactPositions[b]);
                        tDeltaPosition = pl_add_vec3(tDeltaPosition, atLinearChange[d]);

                        // The sign of the change is positive if we're
                        // dealing with the second body in a contact
                        // and negative otherwise (because we're
                        // subtracting the resolution).
                        gptPhysicsCtx->sbtContactArray[i].fPenetration += pl_dot_vec3(tDeltaPosition, gptPhysicsCtx->sbtContactArray[i].tContactNormal) * (b ? 1.0f : -1.0f);
                    }
                }
            }
        }
        uPositionIterationsUsed++;
    }

    static double* pdPosCount = NULL;
    if(!pdPosCount)
        pdPosCount = gptStats->get_counter("physics position its.");
    *pdPosCount = (double)uPositionIterationsUsed;
}

static void
pl__contact_adjust_velocities(float fDeltaTime)
{
    const uint32_t uContactCount = pl_sb_size(gptPhysicsCtx->sbtContactArray);

    plVec3 atVelocityChange[2] = {0};
    plVec3 atRotationChange[2] = {0};
    plVec3 tDeltaVelocity = {0};

    uint32_t uVelocityIterationsUsed = 0;

    float fMax = 0.0f;
    uint32_t uIndex = 0;
    while(uVelocityIterationsUsed < gptPhysicsCtx->tSettings.uMaxVelocityIterations)
    {
        // Find contact with maximum magnitude of probable velocity change.
        fMax = gptPhysicsCtx->tSettings.fVelocityEpsilon;
        uIndex = uContactCount;
        for (uint32_t i = 0; i < uContactCount; i++)
        {
            if (gptPhysicsCtx->sbtContactArray[i].fDesiredDeltaVelocity > fMax)
            {
                fMax = gptPhysicsCtx->sbtContactArray[i].fDesiredDeltaVelocity;
                uIndex = i;
            }
        }
        if (uIndex == uContactCount)
            break;

        // match awake state at contact
        pl__contact_match_awake_state(&gptPhysicsCtx->sbtContactArray[uIndex]);

        // Do the resolution on the contact that came out top.
        pl__contact_apply_velocity_change(&gptPhysicsCtx->sbtContactArray[uIndex], atVelocityChange, atRotationChange);

            // With the change in velocity of the two bodies, the update of
        // contact velocities means that some of the relative closing
        // velocities need recomputing.
        for (uint32_t i = 0; i < uContactCount; i++)
        {
            // Check each body in the contact
            for (uint32_t b = 0; b < 2; b++)
            {
                if(gptPhysicsCtx->sbtContactArray[i].atBodyIndices[b] == UINT32_MAX)
                    continue;

                // Check for a match with each body in the newly
                // resolved contact
                for (uint32_t d = 0; d < 2; d++)
                {
                    if (gptPhysicsCtx->sbtContactArray[i].atBodyIndices[b] == gptPhysicsCtx->sbtContactArray[uIndex].atBodyIndices[d])
                    {
                        tDeltaVelocity = pl_cross_vec3(atRotationChange[d], gptPhysicsCtx->sbtContactArray[i].atRelativeContactPositions[b]);
                        tDeltaVelocity = pl_add_vec3(tDeltaVelocity, atVelocityChange[d]);

                        // The sign of the change is negative if we're dealing
                        // with the second body in a contact.
                        plMat3 tContactToWorldTranspose = pl_mat3_invert(&gptPhysicsCtx->sbtContactArray[i].tContactToWorld);
                        plVec3 tAdjustedVelocity = pl_mul_vec3_scalarf(pl_mul_mat3_vec3(&tContactToWorldTranspose, tDeltaVelocity), (b ? -1.0f : 1.0f));

                        gptPhysicsCtx->sbtContactArray[i].tContactVelocity = pl_add_vec3(
                            gptPhysicsCtx->sbtContactArray[i].tContactVelocity,
                            tAdjustedVelocity
                        );

                        pl__contact_calculate_desired_delta_velocity(&gptPhysicsCtx->sbtContactArray[i], fDeltaTime);
                    }
                }
            }
        }
        uVelocityIterationsUsed++;
    }

    static double* pdVelCount = NULL;
    if(!pdVelCount)
        pdVelCount = gptStats->get_counter("physics velocity its");
    *pdVelCount = (double)uVelocityIterationsUsed;
}

static void
pl__resolve_contacts(float fDeltaTime)
{
    const uint32_t uContactCount = pl_sb_size(gptPhysicsCtx->sbtContactArray);

    static double* pdContactCount = NULL;
    if(!pdContactCount)
        pdContactCount = gptStats->get_counter("physics contacts");
    *pdContactCount = (double)uContactCount;

    // prepare contacts for processing
    for(uint32_t i = 0; i < uContactCount; i++)
    {
        plContact* ptContact = &gptPhysicsCtx->sbtContactArray[i];
        pl__contact_calculate_internals(ptContact, fDeltaTime);
    }

    // resolve the interpenetration problems with the contacts.
    pl__contact_adjust_positions(fDeltaTime);

    // resolve the velocity problems with the contacts.
    pl__contact_adjust_velocities(fDeltaTime);
}

static void
pl__physics_integrate(float fDeltaTime, plRigidBody* atBodies, uint32_t uBodyCount)
{
    for(uint32_t i = 0; i < uBodyCount; i++)
    {
        plRigidBody* ptBody = &atBodies[i];

        if(ptBody->bIsAwake)
        {

            // calculate linear acceleration from force inputs
            ptBody->tLastFrameAcceleration = ptBody->tAcceleration;
            ptBody->tLastFrameAcceleration = pl_add_vec3(ptBody->tLastFrameAcceleration, pl_mul_vec3_scalarf(ptBody->tForceAccumulation, ptBody->fInverseMass));

            // calculate angular acceleration from torque inputs
            plVec3 tAngularAcceleration = pl_mul_mat3_vec3(&ptBody->tInverseInertiaTensorWorld, ptBody->tTorqueAccumulation);

            // adjust velocities

            // update linear velocity from both acceleration and impulse
            ptBody->tLinearVelocity = pl_add_vec3(ptBody->tLinearVelocity, pl_mul_vec3_scalarf(ptBody->tLastFrameAcceleration, fDeltaTime));

            // update angular velocity from both accleration and impulse
            ptBody->tAnglularVelocity = pl_add_vec3(ptBody->tAnglularVelocity, pl_mul_vec3_scalarf(tAngularAcceleration, fDeltaTime));

            // impose drag
            ptBody->tLinearVelocity = pl_mul_vec3_scalarf(ptBody->tLinearVelocity, powf(ptBody->fLinearDamping, fDeltaTime));
            ptBody->tAnglularVelocity = pl_mul_vec3_scalarf(ptBody->tAnglularVelocity, powf(ptBody->fAngularDamping, fDeltaTime));

            // adjust positions

            // update linear position
            ptBody->tPosition = pl_add_vec3(ptBody->tPosition, pl_mul_vec3_scalarf(ptBody->tLinearVelocity, fDeltaTime));

            // update angular position
            plVec4 tQ0 = {fDeltaTime * ptBody->tAnglularVelocity.x, fDeltaTime * ptBody->tAnglularVelocity.y, fDeltaTime * ptBody->tAnglularVelocity.z, 0};
            tQ0 = pl_mul_quat(tQ0, ptBody->tOrientation);
            ptBody->tOrientation.x += tQ0.x * 0.5f;
            ptBody->tOrientation.y += tQ0.y * 0.5f;
            ptBody->tOrientation.z += tQ0.z * 0.5f;
            ptBody->tOrientation.w += tQ0.w * 0.5f;

            // Normalise the orientation, and update the matrices with the new
            // position and orientation
            ptBody->tOrientation = pl_norm_quat(ptBody->tOrientation);

            // calculate the transform matrix for the body.
            ptBody->tTransform = pl_rotation_translation_scale(ptBody->tOrientation, ptBody->tPosition, (plVec3){1.0f, 1.0f, 1.0f});

            pl__transform_inertia_tensor(&ptBody->tInverseInertiaTensorWorld, &ptBody->tOrientation, &ptBody->tInverseInertiaTensor, &ptBody->tTransform);

            // clear accumulators
            ptBody->tForceAccumulation = (plVec3){0};
            ptBody->tTorqueAccumulation = (plVec3){0};

            // Update the kinetic energy store, and possibly put the body to
            // sleep.
            if (ptBody->bCanSleep)
            {
                float fCurrentMotion = pl_dot_vec3(ptBody->tLinearVelocity, ptBody->tLinearVelocity) + pl_dot_vec3(ptBody->tAnglularVelocity, ptBody->tAnglularVelocity);

                float fBias = powf(0.5f, fDeltaTime);
                ptBody->fMotion = fBias * ptBody->fMotion + (1.0f - fBias) * fCurrentMotion;

                if (ptBody->fMotion < gptPhysicsCtx->tSettings.fSleepEpsilon)
                    pl__set_awake(ptBody, false);
                else if (ptBody->fMotion > 10 * gptPhysicsCtx->tSettings.fSleepEpsilon)
                    ptBody->fMotion = 10 * gptPhysicsCtx->tSettings.fSleepEpsilon;
            }

        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] intersection tests
//-----------------------------------------------------------------------------

static bool
pl__intersect_box_half_space(const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptPlane)
{
    float fProjectedRadius = pl__transform_to_axis(ptBox, &ptPlane->tDirection);

    float fBoxDistance = pl_dot_vec3(ptPlane->tDirection, ptBox->tTransform.col[3].xyz) - fProjectedRadius;

    return fBoxDistance <= ptPlane->fOffset;
}

static bool
pl__intersect_box_box(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1)
{
    plVec3 tToCenter = pl_sub_vec3(ptBox1->tTransform.col[3].xyz, ptBox0->tTransform.col[3].xyz);
    return (
        // check on box 0's axes first
        pl__overlap_on_axis(ptBox0, ptBox1, ptBox0->tTransform.col[0].xyz, tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, ptBox0->tTransform.col[1].xyz, tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, ptBox0->tTransform.col[2].xyz, tToCenter) && 

        // box 1
        pl__overlap_on_axis(ptBox0, ptBox1, ptBox1->tTransform.col[0].xyz, tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, ptBox1->tTransform.col[1].xyz, tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, ptBox1->tTransform.col[2].xyz, tToCenter) && 

        // cross products
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[0].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[1].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[2].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[0].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[1].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[2].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[0].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[1].xyz), tToCenter) && 
        pl__overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[2].xyz), tToCenter)
    );
}

static bool
pl__intersect_sphere_half_space(const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane)
{
    float fRadius = ptSphere->fRadius;

    // find the distance from the origin
    float fBallDistance = pl_dot_vec3(ptPlane->tDirection, ptSphere->tTransform.col[3].xyz) - fRadius;

    // check for intersection
    return fBallDistance <= ptPlane->fOffset;
}

static bool
pl__intersect_sphere_sphere(const plCollisionPrimitive* ptSphere0, const plCollisionPrimitive* ptSphere1)
{
    float fRadius0 = ptSphere0->fRadius;
    float fRadius1 = ptSphere1->fRadius;

    // find the vector between the objects
    plVec3 tMidLine = pl_sub_vec3(ptSphere0->tTransform.col[3].xyz, ptSphere1->tTransform.col[3].xyz);

    // see if it is large enough
    return pl_length_sqr_vec3(tMidLine) < (fRadius0 + fRadius1) * (fRadius0 + fRadius1);
}

//-----------------------------------------------------------------------------
// [SECTION] collision detection
//-----------------------------------------------------------------------------

static void
pl__collision_sphere_half_space(const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane)
{
    // cache sphere position
    plVec3 tPosition = ptSphere->tTransform.col[3].xyz;

    float fRadius = ptSphere->fRadius;

    // find distance from plane
    const float fBallDistance = pl_dot_vec3(ptPlane->tDirection, tPosition) - fRadius - ptPlane->fOffset;

    if(fBallDistance >= 0)
        return;

    // create contact
    plContact tContact = {
        .tContactNormal = ptPlane->tDirection,
        .fPenetration   = -fBallDistance,
        .tContactPoint  = pl_sub_vec3(tPosition, pl_mul_vec3_scalarf(ptPlane->tDirection, fBallDistance + fRadius)),
        .fRestitution   = gptPhysicsCtx->fRestitution,
        .fFriction      = ptSphere->fFriction,
        .atBodyIndices  = {
            ptSphere->uBodyIndex,
            UINT32_MAX
        }
    };
    pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
}

static void
pl__collision_sphere_true_plane(const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane)
{
    // cache sphere position
    plVec3 tPosition = ptSphere->tTransform.col[3].xyz;

    float fRadius = ptSphere->fRadius;

    // find distance from plane
    const float fCenterDistance = pl_dot_vec3(ptPlane->tDirection, tPosition) - ptPlane->fOffset;

    // check if within radius
    if(fCenterDistance * fCenterDistance > fRadius * fRadius)
        return;

    // check which side of plane we're on
    plVec3 tNormal = ptPlane->tDirection;
    float fPenetration = -fCenterDistance;
    if(fCenterDistance < 0)
    {
        tNormal = pl_mul_vec3_scalarf(tNormal, -1.0f);
        fPenetration = -fPenetration;
    }
    fPenetration += fRadius;

    // create contact
    plContact tContact = {
        .tContactNormal = tNormal,
        .fPenetration   = fPenetration,
        .tContactPoint  = pl_sub_vec3(tPosition, pl_mul_vec3_scalarf(ptPlane->tDirection, fCenterDistance)),
        .fRestitution   = gptPhysicsCtx->fRestitution,
        .fFriction      = ptSphere->fFriction,
        .atBodyIndices  = {
            ptSphere->uBodyIndex,
            UINT32_MAX
        }
    };
    pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
}

static void
pl__collision_sphere_sphere(const plCollisionPrimitive* ptSphere0, const plCollisionPrimitive* ptSphere1)
{
    // cache sphere positions
    plVec3 tPosition0 = ptSphere0->tTransform.col[3].xyz;
    plVec3 tPosition1 = ptSphere1->tTransform.col[3].xyz;

    float fRadius0 = ptSphere0->fRadius;
    float fRadius1 = ptSphere1->fRadius;

    // find vector between objects
    plVec3 tMidLine = pl_sub_vec3(tPosition0, tPosition1);
    float fSize = pl_length_vec3(tMidLine);

    // see if large enough
    if(fSize <= 0.0f || fSize >= fRadius0 + fRadius1)
        return;

    // manually create normal because we have size already
    plVec3 tNormal = pl_mul_vec3_scalarf(tMidLine, 1.0f / fSize);

    // create contact
    plContact tContact = {
        .tContactNormal = tNormal,
        .fPenetration   = fRadius0 + fRadius1 - fSize,
        .tContactPoint  = pl_add_vec3(tPosition0, pl_mul_vec3_scalarf(tMidLine, 0.5f)),
        .fRestitution   = gptPhysicsCtx->fRestitution,
        .fFriction      = ptSphere0->fFriction,
        .atBodyIndices  = {
            ptSphere0->uBodyIndex,
            ptSphere1->uBodyIndex
        }
    };
    pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
}

static void
pl__collision_box_half_space(const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptPlane)
{
    // check for intersection
    if(!pl__intersect_box_half_space(ptBox, ptPlane))
        return;

    // We have an intersection, so find the intersection points. We can make
    // do with only checking vertices. If the box is resting on a plane
    // or on an edge, it will be reported as four or two contact points.

    // Go through each combination of + and - for each half-size
    static float atOffsets[8][3] = {
        {  1.0f,  1.0f,  1.0f},
        { -1.0f,  1.0f,  1.0f},
        {  1.0f, -1.0f,  1.0f},
        { -1.0f, -1.0f,  1.0f},
        {  1.0f,  1.0f, -1.0f},
        { -1.0f,  1.0f, -1.0f},
        {  1.0f, -1.0f, -1.0f},
        { -1.0f, -1.0f, -1.0f}
    }; 

    for(uint32_t i = 0; i < 8; i++)
    {
        // calculate position of each vertex
        plVec3 tVertexPos = {
            atOffsets[i][0] * ptBox->tHalfSize.x,
            atOffsets[i][1] * ptBox->tHalfSize.y,
            atOffsets[i][2] * ptBox->tHalfSize.z
        };
        tVertexPos = pl_mul_mat4_vec3(&ptBox->tTransform, tVertexPos);

        // calculate distance from plane
        float fVertexDistance = pl_dot_vec3(tVertexPos, ptPlane->tDirection);

        // compare to distance from plane
        if(fVertexDistance <= ptPlane->fOffset)
        {
            // create contact info

            // create contact
            plContact tContact = {
                .tContactNormal = ptPlane->tDirection,
                .fPenetration   = ptPlane->fOffset - fVertexDistance,
                .tContactPoint  = pl_add_vec3(tVertexPos, pl_mul_vec3_scalarf(ptPlane->tDirection, fVertexDistance - ptPlane->fOffset)),
                .fRestitution   = gptPhysicsCtx->fRestitution,
                .fFriction      = ptBox->fFriction,
                .atBodyIndices  = {
                    ptBox->uBodyIndex,
                    UINT32_MAX
                }
            };
            pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
        }
    }
}

static inline float
pl__penetration_on_axis(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1, const plVec3* ptAxis, const plVec3* ptToCenter)
{
    // project half-size of one onto axis
    float fOneProject = pl__transform_to_axis(ptBox0, ptAxis);
    float fTwoProject = pl__transform_to_axis(ptBox1, ptAxis);

    // project this onto axis
    float fDistance = fabsf(pl_dot_vec3(*ptToCenter, *ptAxis));

    // return overlap
    return fOneProject + fTwoProject - fDistance;
}

static inline bool
pl__try_axis(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1, plVec3 tAxis,
    const plVec3* ptToCenter, uint32_t uIndex, float* pfSmallestPenetration, uint32_t* puSmallestCase)
{
    // make sure we have normalized axis and don't check almost parallel axes
    if(pl_length_sqr_vec3(tAxis) < 0.0001f)
        return true;
    
    tAxis = pl_norm_vec3(tAxis);

    float fPenetration = pl__penetration_on_axis(ptBox0, ptBox1, &tAxis, ptToCenter);

    if(fPenetration < 0.0f)
        return false;
    if(fPenetration < *pfSmallestPenetration)
    {
        *pfSmallestPenetration = fPenetration;
        *puSmallestCase = uIndex;
    }
    return true;
}

static inline void
pl__fill_point_face_box_box(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1,
    const plVec3* ptToCenter, uint32_t uBest, float fPenetration)
{
    // we know which axis collision is on (i.e best), but
    // we need to work out which of the two faces
    plVec3 tNormal = ptBox0->tTransform.col[uBest].xyz;
    if(pl_dot_vec3(tNormal, *ptToCenter) > 0.0f)
    {
        tNormal = pl_mul_vec3_scalarf(tNormal, -1.0f);
    }

    // work out which vertex of box 1 we're colliding with.
    plVec3 tVertex = ptBox1->tHalfSize;
    if(pl_dot_vec3(ptBox1->tTransform.col[0].xyz, *ptToCenter) < 0.0f) tVertex.x = -tVertex.x;
    if(pl_dot_vec3(ptBox1->tTransform.col[1].xyz, *ptToCenter) < 0.0f) tVertex.y = -tVertex.y;
    if(pl_dot_vec3(ptBox1->tTransform.col[2].xyz, *ptToCenter) < 0.0f) tVertex.z = -tVertex.z;

    // create contact
    plContact tContact = {
        .tContactNormal = tNormal,
        .fPenetration   = fPenetration,
        .tContactPoint  = pl_mul_mat4_vec3(&ptBox1->tTransform, tVertex),
        .fRestitution   = gptPhysicsCtx->fRestitution,
        .fFriction      = ptBox0->fFriction,
        .atBodyIndices  = {
            ptBox0->uBodyIndex,
            ptBox1->uBodyIndex
        }
    };
    pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
}

static inline plVec3
pl__contact_point(const plVec3* ptPOne, const plVec3* ptDOne, float fOneSize, const plVec3* ptPTwo, const plVec3* ptDTwo, float fTwoSize, bool bUseOne)
{
    
    float fSmOne = pl_length_sqr_vec3(*ptDOne);
    float fSmTwo = pl_length_sqr_vec3(*ptDTwo);
    float fDpOneTwo = pl_dot_vec3(*ptDTwo, *ptDOne);

    plVec3 tToSt = pl_sub_vec3(*ptPOne, *ptPTwo);
    float fDpStAOne = pl_dot_vec3(*ptDOne, tToSt);
    float fDpStATwo = pl_dot_vec3(*ptDTwo, tToSt);

    float fDenom = fSmOne * fSmTwo - fDpOneTwo * fDpOneTwo;

    // zero denominator indicates parallel lines
    if(fabsf(fDenom) < 0.0001f)
        return bUseOne ? *ptPOne : *ptPTwo;

    float fMua = (fDpOneTwo * fDpStATwo - fSmTwo * fDpStAOne) / fDenom;
    float fMub = (fSmOne * fDpStATwo - fDpOneTwo * fDpStAOne) / fDenom;

    // If either of the edges has the nearest point out
    // of bounds, then the edges aren't crossed, we have
    // an edge-face contact. Our point is on the edge, which
    // we know from the useOne parameter.
    if (fMua > fOneSize ||
        fMua < -fOneSize ||
        fMub > fTwoSize ||
        fMub < -fTwoSize)
    {
        return bUseOne ? *ptPOne : *ptPTwo;
    }
    else
    {
        plVec3 tCOne = pl_add_vec3( *ptPOne, pl_mul_vec3_scalarf(*ptDOne, fMua));
        plVec3 tCTwo = pl_add_vec3( *ptPTwo, pl_mul_vec3_scalarf(*ptDTwo, fMub));
        return pl_add_vec3(pl_mul_vec3_scalarf(tCOne, 0.5f), pl_mul_vec3_scalarf(tCTwo, 0.5f));
    }
}

static void
pl__collision_box_box(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1)
{
    // find vector between centers
    plVec3 tToCenter = pl_sub_vec3(ptBox1->tTransform.col[3].xyz, ptBox0->tTransform.col[3].xyz);

    // start by assuming no contact
    float fPen = FLT_MAX;
    uint32_t uBest = UINT32_MAX;

    // new check each axes, returning if it gives us a
    // separating axis, and keeping track of axis with smallest
    // penetration otherwise
    if(!pl__try_axis(ptBox0, ptBox1, ptBox0->tTransform.col[0].xyz, &tToCenter, 0, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, ptBox0->tTransform.col[1].xyz, &tToCenter, 1, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, ptBox0->tTransform.col[2].xyz, &tToCenter, 2, &fPen, &uBest)) return;

    if(!pl__try_axis(ptBox0, ptBox1, ptBox1->tTransform.col[0].xyz, &tToCenter, 3, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, ptBox1->tTransform.col[1].xyz, &tToCenter, 4, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, ptBox1->tTransform.col[2].xyz, &tToCenter, 5, &fPen, &uBest)) return;

    // store best axis-major in case we run into almost
    // parallel edge collisions later
    uint32_t uBestSingleAxis = uBest;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[0].xyz), &tToCenter,  6, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[1].xyz), &tToCenter,  7, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[2].xyz), &tToCenter,  8, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[0].xyz), &tToCenter,  9, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[1].xyz), &tToCenter, 10, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[2].xyz), &tToCenter, 11, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[0].xyz), &tToCenter, 12, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[1].xyz), &tToCenter, 13, &fPen, &uBest)) return;
    if(!pl__try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[2].xyz), &tToCenter, 14, &fPen, &uBest)) return;

    // make sure we have a result
    PL_ASSERT(uBest != UINT32_MAX);

    // we not know there's a collision, and we know which
    // axes gave smallest penetration. We now can deal with it
    // depending on the case
    if(uBest < 3)
    {
        // We've got a vertex of box two on a face of box one.
        pl__fill_point_face_box_box(ptBox0, ptBox1, &tToCenter, uBest, fPen);
        return;
    }
    else if(uBest < 6)
    {
        // We've got a vertex of box one on a face of box two.
        // We use the same algorithm as above, but swap around
        // one and two (and therefore also the vector between their
        // centres).
        tToCenter = pl_mul_vec3_scalarf(tToCenter, -1.0f);
        pl__fill_point_face_box_box(ptBox1, ptBox0, &tToCenter, uBest - 3, fPen);
        return;
    }
    else
    {
        // We've got an edge-edge contact. Find out which axes
        uBest -= 6;
        uint32_t uOneAxisIndex = uBest / 3;
        uint32_t uTwoAxisIndex = uBest % 3;

        plVec3 tOneAxis = ptBox0->tTransform.col[uOneAxisIndex].xyz;
        plVec3 tTwoAxis = ptBox1->tTransform.col[uTwoAxisIndex].xyz;
        plVec3 tAxis = pl_norm_vec3(pl_cross_vec3(tOneAxis, tTwoAxis));

        // The axis should point from box one to box two.
        if (pl_dot_vec3(tAxis, tToCenter) > 0.0f)
            tAxis = pl_mul_vec3_scalarf(tAxis, -1.0f);

        // We have the axes, but not the edges: each axis has 4 edges parallel
        // to it, we need to find which of the 4 for each object. We do
        // that by finding the point in the centre of the edge. We know
        // its component in the direction of the box's collision axis is zero
        // (its a mid-point) and we determine which of the extremes in each
        // of the other axes is closest.
        plVec3 tPointOnOneEdge = ptBox0->tHalfSize;
        plVec3 tPointOnTwoEdge = ptBox1->tHalfSize;
        for(uint32_t i = 0; i < 3; i++)
        {
            if(i == uOneAxisIndex)
                tPointOnOneEdge.d[i] = 0;
            else if(pl_dot_vec3(ptBox0->tTransform.col[i].xyz, tAxis) > 0.0f)
                tPointOnOneEdge.d[i] = -tPointOnOneEdge.d[i];

            if(i == uTwoAxisIndex)
                tPointOnTwoEdge.d[i] = 0;
            else if(pl_dot_vec3(ptBox1->tTransform.col[i].xyz, tAxis) < 0.0f)
                tPointOnTwoEdge.d[i] = -tPointOnTwoEdge.d[i];
        }

        // Move them into world coordinates (they are already oriented
        // correctly, since they have been derived from the axes).
        tPointOnOneEdge = pl_mul_mat4_vec3(&ptBox0->tTransform, tPointOnOneEdge);
        tPointOnTwoEdge = pl_mul_mat4_vec3(&ptBox1->tTransform, tPointOnTwoEdge);

        // So we have a point and a direction for the colliding edges.
        // We need to find out point of closest approach of the two
        // line-segments.
        plVec3 tVertex = pl__contact_point(
            &tPointOnOneEdge, &tOneAxis, ptBox0->tHalfSize.d[uOneAxisIndex],
            &tPointOnTwoEdge, &tTwoAxis, ptBox1->tHalfSize.d[uTwoAxisIndex],
            uBestSingleAxis > 2);

        // create contact
        plContact tContact = {
            .tContactNormal = tAxis,
            .fPenetration   = fPen,
            .tContactPoint  = tVertex,
            .fRestitution   = gptPhysicsCtx->fRestitution,
            .fFriction      = ptBox0->fFriction,
            .atBodyIndices  = {
                ptBox0->uBodyIndex,
                ptBox1->uBodyIndex
            }
        };
        pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
    }
}

static void
pl__collision_box_point(const plCollisionPrimitive* ptBox, plVec3 tPoint)
{
    // transform point into box coordinates
    plMat4 tInverseTransform = pl_mat4_invert(&ptBox->tTransform);
    plVec3 tRelPoint = pl_mul_mat4_vec3(&tInverseTransform, tPoint);

    plVec3 tNormal = {0};

    // check each axis, looking for axis with most shallow penetration
    float fMinDepth = ptBox->tHalfSize.x - fabsf(tRelPoint.x);
    if(fMinDepth < 0.0f)
        return;
    tNormal = pl_mul_vec3_scalarf(ptBox->tTransform.col[0].xyz, tRelPoint.x < 0.0f ? -1.0f : 1.0f);

    float fDepth = ptBox->tHalfSize.y - fabsf(tRelPoint.y);
    if(fDepth < 0.0f)
        return;
    else if(fDepth < fMinDepth)
    {
        fMinDepth = fDepth;
        tNormal = pl_mul_vec3_scalarf(ptBox->tTransform.col[1].xyz, tRelPoint.y < 0.0f ? -1.0f : 1.0f);
    }

    fDepth = ptBox->tHalfSize.z - fabsf(tRelPoint.z);
    if(fDepth < 0.0f)
        return;
    else if(fDepth < fMinDepth)
    {
        fMinDepth = fDepth;
        tNormal = pl_mul_vec3_scalarf(ptBox->tTransform.col[2].xyz, tRelPoint.z < 0.0f ? -1.0f : 1.0f);
    }

    // create contact
    plContact tContact = {
        .tContactNormal = tNormal,
        .fPenetration   = fMinDepth,
        .tContactPoint  = tPoint,
        .fRestitution   = gptPhysicsCtx->fRestitution,
        .fFriction      = ptBox->fFriction,
        .atBodyIndices  = {
            ptBox->uBodyIndex,
            UINT32_MAX
        }
    };
    pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
}

static void
pl__collision_box_sphere(const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptSphere)
{
    // transform sphere center into box coordinates
    plVec3 tCenter = ptSphere->tTransform.col[3].xyz;
    plMat4 tInverseTransform = pl_mat4_invert(&ptBox->tTransform);
    plVec3 tRelCenter = pl_mul_mat4_vec3(&tInverseTransform, tCenter);
    // plVec3 tRelCenter = pl_sub_vec3(tCenter, ptBox->tTransform.col[3].xyz);

    float fRadius = ptSphere->fRadius;

    plVec3 tHalfSize = {
        ptBox->tHalfSize.x,
        ptBox->tHalfSize.y,
        ptBox->tHalfSize.z
    };

    // early out check
    if(fabsf(tRelCenter.x) - fRadius > tHalfSize.x ||
       fabsf(tRelCenter.y) - fRadius > tHalfSize.y ||
       fabsf(tRelCenter.z) - fRadius > tHalfSize.z)
    {
        return;
    }

    plVec3 tClosestPoint = {0};
    float fDist = 0.0f;

    // clamp each coordinate to box
    fDist = tRelCenter.x;
    if(fDist > tHalfSize.x)  fDist = tHalfSize.x;
    if(fDist < -tHalfSize.x) fDist = -tHalfSize.x;
    tClosestPoint.x = fDist;

    fDist = tRelCenter.y;
    if(fDist > tHalfSize.y)  fDist = tHalfSize.y;
    if(fDist < -tHalfSize.y) fDist = -tHalfSize.y;
    tClosestPoint.y = fDist;

    fDist = tRelCenter.z;
    if(fDist > tHalfSize.z)  fDist = tHalfSize.z;
    if(fDist < -tHalfSize.z) fDist = -tHalfSize.z;
    tClosestPoint.z = fDist;

    // check we're in contact
    fDist = pl_length_sqr_vec3(pl_sub_vec3(tClosestPoint, tRelCenter));
    if(fDist > fRadius * fRadius)
        return;

    // compile contact
    plVec3 tClosestPointWorld = pl_mul_mat4_vec3(&ptBox->tTransform, tClosestPoint);

    // create contact
    plContact tContact = {
        .tContactNormal = pl_norm_vec3(pl_sub_vec3(tClosestPointWorld, tCenter)),
        .fPenetration   = fRadius - sqrtf(fDist),
        .tContactPoint  = tClosestPointWorld,
        .fRestitution   = gptPhysicsCtx->fRestitution,
        .fFriction      = ptBox->fFriction,
        .atBodyIndices  = {
            ptBox->uBodyIndex,
            ptSphere->uBodyIndex
        }
    };
    pl_sb_push(gptPhysicsCtx->sbtContactArray, tContact);
}

//-----------------------------------------------------------------------------
// [SECTION] contact
//-----------------------------------------------------------------------------

static plVec3
pl__contact_calculate_local_velocity(plContact* ptContact, uint32_t uBodyIndex, float fDeltaTime)
{
    plRigidBody* ptThisBody = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[uBodyIndex]];

    // work out velocity of contact point
    plVec3 tVelocity = pl_cross_vec3(ptThisBody->tAnglularVelocity, ptContact->atRelativeContactPositions[uBodyIndex]);
    tVelocity = pl_add_vec3(tVelocity, ptThisBody->tLinearVelocity);

    // turn velocity info contact coordinates
    plMat3 tContactToWorldTranspose = pl_mat3_invert(&ptContact->tContactToWorld);
    plVec3 tContactVelocity = pl_mul_mat3_vec3(&tContactToWorldTranspose, tVelocity);

    // calculate amount of velocity due to forces without reactions
    plVec3 tAccVelocity = pl_mul_vec3_scalarf(ptThisBody->tLastFrameAcceleration, fDeltaTime);

    tAccVelocity = pl_mul_mat3_vec3(&tContactToWorldTranspose, tAccVelocity);

    // ignore any component of acceleration in the contact normal diraction, we
    // are only interested in planar accelerations
    tAccVelocity.x = 0.0f;

    // add planar velocities - if there's enough friction they will be removed
    // during velocity resolution
    tContactVelocity = pl_add_vec3(tContactVelocity, tAccVelocity);

    return tContactVelocity;
}

static void
pl__contact_calculate_desired_delta_velocity(plContact* ptContact, float fDeltaTime)
{
    const static float fVelocityLimit = 0.25f;

    // calculate acceleration induced velocity accumulated this frame
    float fVelocityFromAcc = 0.0f;

    plRigidBody* ptBody0 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[0]];
    plRigidBody* ptBody1 = ptContact->atBodyIndices[1] == UINT32_MAX ? NULL : &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[1]];
    if(ptBody0->bIsAwake)
    {
        fVelocityFromAcc += pl_dot_vec3(ptBody0->tLastFrameAcceleration, pl_mul_vec3_scalarf(ptContact->tContactNormal, fDeltaTime));
    }

    if(ptBody1 && ptBody1->bIsAwake)
    {
        fVelocityFromAcc -= pl_dot_vec3(ptContact->tContactNormal, pl_mul_vec3_scalarf(ptBody1->tLastFrameAcceleration, fDeltaTime));
    }

    // if velocity is very slow, limit restituion
    float fThisRestitution = ptContact->fRestitution;
    if(fabsf(ptContact->tContactVelocity.x) < fVelocityLimit)
    {
        fThisRestitution = 0.0f;
    }

    // combine bounce velocity with removed acceleration velocity
    ptContact->fDesiredDeltaVelocity = -ptContact->tContactVelocity.x - fThisRestitution * (ptContact->tContactVelocity.x - fVelocityFromAcc);
}

static void
pl__contact_calculate_internals(plContact* ptContact, float fDeltaTime)
{

    // check if first object is NULL and swap if it is
    if(ptContact->atBodyIndices[0] == UINT32_MAX)
    {
        ptContact->atBodyIndices[0] = ptContact->atBodyIndices[1];
        ptContact->atBodyIndices[1] = UINT32_MAX;
    }

    plRigidBody* ptBody0 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[0]];
    plRigidBody* ptBody1 = NULL;

    if(ptContact->atBodyIndices[1] != UINT32_MAX)
        ptBody1 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[1]];

    // calculate a set of axis at the contact point
    pl__contact_calculate_contact_basis(ptContact);

    // store the relative position of the contact relative to each body
    ptContact->atRelativeContactPositions[0] = pl_sub_vec3(ptContact->tContactPoint, ptBody0->tPosition);
    if(ptBody1)
        ptContact->atRelativeContactPositions[1] = pl_sub_vec3(ptContact->tContactPoint, ptBody1->tPosition);

    // find relative velocity of bodies at contact point
    ptContact->tContactVelocity = pl__contact_calculate_local_velocity(ptContact, 0, fDeltaTime);
    if(ptBody1)
    {
        ptContact->tContactVelocity = pl_sub_vec3(ptContact->tContactVelocity, pl__contact_calculate_local_velocity(ptContact, 1, fDeltaTime));
    }

    pl__contact_calculate_desired_delta_velocity(ptContact, fDeltaTime);
}

static void
pl__contact_calculate_contact_basis(plContact* ptContact)
{
    plVec3 atContactTangent[2] = {0};

    // check whether z-axis is nearer to x or y axis
    if(fabsf(ptContact->tContactNormal.x) > fabsf(ptContact->tContactNormal.y))
    {
        // scalling factor to ensure results are normalized
        const float fS = 1.0f / sqrtf(ptContact->tContactNormal.z * ptContact->tContactNormal.z +
            ptContact->tContactNormal.x * ptContact->tContactNormal.x);

        // new x-axis is at right angle to world y-axis
        atContactTangent[0].x = ptContact->tContactNormal.z * fS;
        atContactTangent[0].y = 0.0f;
        atContactTangent[0].z = -ptContact->tContactNormal.x * fS;

        // new y-axis is at right angle to new x and z axes
        atContactTangent[1].x = ptContact->tContactNormal.y * atContactTangent[0].z;
        atContactTangent[1].y = ptContact->tContactNormal.z * atContactTangent[0].x - ptContact->tContactNormal.x * atContactTangent[0].z;
        atContactTangent[1].z = -ptContact->tContactNormal.y * atContactTangent[0].x;
    }
    else
    {
        // scalling factor to ensure results are normalized
        const float fS = 1.0f / sqrtf(ptContact->tContactNormal.z * ptContact->tContactNormal.z +
            ptContact->tContactNormal.y * ptContact->tContactNormal.y);

        // new x-axis is at right angle to world y-axis
        atContactTangent[0].x = 0.0f;
        atContactTangent[0].y = -ptContact->tContactNormal.z * fS;
        atContactTangent[0].z = ptContact->tContactNormal.y * fS;

        // new t-axis is at right angle to new x and z axes
        atContactTangent[1].x = ptContact->tContactNormal.y * atContactTangent[0].z - ptContact->tContactNormal.z * atContactTangent[0].y;
        atContactTangent[1].y = -ptContact->tContactNormal.x * atContactTangent[0].z;
        atContactTangent[1].z = ptContact->tContactNormal.x * atContactTangent[0].y;
    }

    ptContact->tContactToWorld.col[0] = ptContact->tContactNormal;
    ptContact->tContactToWorld.col[1] = atContactTangent[1];
    ptContact->tContactToWorld.col[2] = atContactTangent[0];
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_physics_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plPhysicsI tApi = {
        .initialize                  = pl_physics_initialize,
        .cleanup                     = pl_physics_cleanup,
        .reset                       = pl_physics_reset,
        .set_settings                = pl_physics_set_settings,
        .get_settings                = pl_physics_get_settings,
        .update                      = pl_physics_update,
        .draw                        = pl_physics_draw,
        .apply_force                 = pl_physics_apply_force,
        .apply_force_at_point        = pl_physics_apply_force_at_point,
        .apply_force_at_body_point   = pl_physics_apply_force_at_body_point,
        .apply_impulse               = pl_physics_apply_impulse,
        .apply_impulse_at_point      = pl_physics_apply_impulse_at_point,
        .apply_impulse_at_body_point = pl_physics_apply_impulse_at_body_point,
        .apply_torque                = pl_physics_apply_torque,
        .wake_up_body                = pl_physics_wake_up_body,
        .wake_up_all                 = pl_physics_wake_up_all,
        .sleep_body                  = pl_physics_sleep_body,
        .sleep_all                   = pl_physics_sleep_all,
    };
    pl_set_api(ptApiRegistry, plPhysicsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory  = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptECS     = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptDraw    = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptStats   = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptLog     = pl_get_api_latest(ptApiRegistry, plLogI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptPhysicsCtx = ptDataRegistry->get_data("plPhysicsContext");
    }
    else // first load
    {
        static plPhysicsContext gtPhysicsCtx = {
            0
        };
        gptPhysicsCtx = &gtPhysicsCtx;
        ptDataRegistry->set_data("plPhysicsContext", gptPhysicsCtx);
    }
}

PL_EXPORT void
pl_unload_physics_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plPhysicsI* ptApi = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    ptApiRegistry->remove_api(ptApi);

    pl_sb_free(gptPhysicsCtx->sbtRigidBodies);
}
