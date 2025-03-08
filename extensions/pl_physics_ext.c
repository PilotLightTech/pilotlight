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
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h>
#include <stdbool.h>
#include "pl.h"
#include "pl_physics_ext.h"

// extensions
#include "pl_console_ext.h"
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

    static const plEcsI* gptECS = NULL;
    static const plConsoleI* gptConsole = NULL;
    static const plDrawI* gptDraw = NULL;
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
typedef struct _plCollisionData      plCollisionData;
typedef struct _plCollisionPrimitive plCollisionPrimitive;


// enums
typedef int plCollisionPrimitiveType;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plCollisionPrimitiveType
{
    PL_COLLISION_PRIMITIVE_TYPE_BOX,
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
    plMat4   tInverseInertiaTensor;
    plMat4   tInverseInertiaTensorWorld;
    plVec3   tLinearVelocity;
    plVec3   tAnglularVelocity;
    plVec3   tLastFrameAcceleration;
    plVec3   tAcceleration;
    float    fRadius;
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
    plMat4   tContactToWorld;
    plVec3   tContactVelocity;
    float    fDesiredDeltaVelocity;
    plVec3   atRelativeContactPositions[2];
} plContact;

typedef struct _plContactResolver
{
    uint32_t uVelocityIterations;
    uint32_t uPositionIterations;
    float    fVelocityEpsilon;
    float    fPositionEpsilon;
    uint32_t uPositionIterationsUsed;
    uint32_t uVelocityIterationsUsed;
    bool     bValidSettings;
} plContactResolver;

typedef struct _plCollisionData
{
    plContact* sbtContactArray;
    float      fFriction;
    float      fRestitution;
    float      fTolerance;
} plCollisionData;

typedef struct _plCollisionPrimitive
{
    plCollisionPrimitiveType tType;
    uint32_t                 uBodyIndex;
    plMat4                   tOffset;
    plMat4                   tTransform;
  
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
    bool            bActive;
    plRigidBody*    sbtRigidBodies;
    plCollisionData tCollisionData;
} plPhysicsContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plPhysicsContext* gptPhysicsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// intersection tests
static bool pl__intersect_box_half_space   (const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptPlane);
static bool pl__intersect_box_box          (const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1);
static bool pl__intersect_sphere_half_space(const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane);
static bool pl__intersect_sphere_sphere    (const plCollisionPrimitive* ptSphere0, const plCollisionPrimitive* ptSphere1);

// collision detection
static void pl__collision_sphere_half_space(plCollisionData*, const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane);
static void pl__collision_sphere_true_plane(plCollisionData*, const plCollisionPrimitive* ptSphere, const plCollisionPrimitive* ptPlane);
static void pl__collision_sphere_sphere    (plCollisionData*, const plCollisionPrimitive* ptSphere0, const plCollisionPrimitive* ptSphere1);
static void pl__collision_box_half_space   (plCollisionData*, const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptPlane);
static void pl__collision_box_box          (plCollisionData*, const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1);
static void pl__collision_box_point        (plCollisionData*, const plCollisionPrimitive* ptBox, plVec3 tPoint);
static void pl__collision_box_sphere       (plCollisionData*, const plCollisionPrimitive* ptBox, const plCollisionPrimitive* ptSphere);

// contact
static void pl__contact_calculate_internals             (plContact*, float fDeltaTime);
static void pl__contact_calculate_desired_delta_velocity(plContact*, float fDeltaTime);
static void pl__contact_calculate_local_velocity        (plContact*, float fDeltaTime);
static void pl__contact_calculate_contact_basis         (plContact*);
static void pl__contact_apply_velocity_change           (plContact*, plVec3 atVelocityChange[2], plVec3 atRotationChange[2]);
static void pl__contact_apply_position_change           (plContact*, plVec3 atLinearChange[2], plVec3 atAngularChange[2], float fPenetration);
static void pl__contact_calculate_frictionless_impulse  (plContact*, plMat4* ptInverseInertiaTensor);
static void pl__contact_calculate_friction_impulse      (plContact*, plMat4* ptInverseInertiaTensor);

// contact resolver
static void  pl__resolve_contacts (plContactResolver*, plContact*, uint32_t uContactCount, float fDeltaTime);
static void  pl__prepare_contacts (plContactResolver*, plContact*, uint32_t uContactCount, float fDeltaTime);
static void  pl__adjust_velocities(plContactResolver*, plContact*, uint32_t uContactCount, float fDeltaTime);
static void  pl__adjust_positions (plContactResolver*, plContact*, uint32_t uContactCount, float fDeltaTime);

// misc
static bool  pl__overlap_on_axis(const plCollisionPrimitive* ptBox0, const plCollisionPrimitive* ptBox1, plVec3 tAxis, plVec3 tToCenter);
static float pl__transform_to_axis(const plCollisionPrimitive* ptBox, const plVec3* ptAxis);
static void  pl__physics_integrate(float fDeltaTime, plRigidBody* atBodies, uint32_t uBodyCount);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_physics_initialize(void)
{
    gptConsole->add_bool_variable("p.PhysicsOn", &gptPhysicsCtx->bActive, "controls physics simulation", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
}

void
pl_physics_cleanup(void)
{

}

void
pl_physics_update(float fDeltaTime, plComponentLibrary* ptLibrary)
{

    if(!gptPhysicsCtx->bActive)
        return;

    plEntity* sbtRigidBodyEntities = ptLibrary->tRigidBodyPhysicsComponentManager.sbtEntities;
    const uint32_t uRigidBodyCount = pl_sb_size(sbtRigidBodyEntities);
    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plEntity tEntity = sbtRigidBodyEntities[i];
        plRigidBodyPhysicsComponent* ptRigidBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);
        if(ptRigidBody->uPhysicsObject == UINT64_MAX)
        {
            ptRigidBody->uPhysicsObject = pl_sb_size(gptPhysicsCtx->sbtRigidBodies);

            plRigidBody tBody = {
                .fLinearDamping  = 1.0f - ptRigidBody->fLinearDamping,
                .fInverseMass    = 1.0f / ptRigidBody->fMass,
                .tPosition       = ptTransform->tTranslation,
                .tOrientation    = ptTransform->tRotation,
                .tAcceleration   = ptRigidBody->tGravity,
                .tLinearVelocity = (plVec3){0},
                .tEntity         = tEntity,
                .fRadius         = ptRigidBody->fRadius,
                .fAngularDamping = 1.0f - ptRigidBody->fAngularDamping,
                .bIsAwake        = true
            };

            plVec3 tSquares = pl_mul_vec3_scalarf(ptRigidBody->tExtents, 0.5f);
            tSquares = pl_mul_vec3(tSquares, tSquares);

            float ix = 0.3f * ptRigidBody->fMass * (tSquares.y + tSquares.z);
            float iy = 0.3f * ptRigidBody->fMass * (tSquares.x + tSquares.z);
            float iz = 0.3f * ptRigidBody->fMass * (tSquares.x + tSquares.y);

            plMat4 tInertiaTensor = {0};

            tInertiaTensor.col[0].x = ix;
            tInertiaTensor.col[1].y  = iy;
            tInertiaTensor.col[2].z  = iz;
            tInertiaTensor.col[3].w  = 1.0f;
            tBody.tInverseInertiaTensor = pl_mat4_invert(&tInertiaTensor);


            pl_sb_push(gptPhysicsCtx->sbtRigidBodies, tBody);
        }
        else
        {
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].fLinearDamping = 1.0f - ptRigidBody->fLinearDamping;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].fAngularDamping = 1.0f - ptRigidBody->fAngularDamping;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].fInverseMass = 1.0f / ptRigidBody->fMass;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tPosition = ptTransform->tTranslation;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tOrientation = ptTransform->tRotation;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].tAcceleration = ptRigidBody->tGravity;
            gptPhysicsCtx->sbtRigidBodies[ptRigidBody->uPhysicsObject].fRadius = ptRigidBody->fRadius;
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
            for(uint32_t j = 0; j < uRigidBodyCount; j++)
            {
                plRigidBody* ptParticle = &gptPhysicsCtx->sbtRigidBodies[j];

                plVec3 tDirection = pl_sub_vec3(ptParticle->tPosition, ptTransform->tTranslation);

                float fDistance2 = pl_length_sqr_vec3(tDirection);

                if(fDistance2 < ptForceField->fRange * ptForceField->fRange)
                {
                    tDirection = pl_norm_vec3(tDirection);
                    ptParticle->tForceAccumulation = pl_add_vec3(ptParticle->tForceAccumulation, pl_mul_vec3_scalarf(tDirection, -ptForceField->fGravity / ptParticle->fInverseMass));
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
                }
            }
        }
    }

    pl__physics_integrate(fDeltaTime, gptPhysicsCtx->sbtRigidBodies, uRigidBodyCount);

    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plTransformComponent* ptSphereTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, gptPhysicsCtx->sbtRigidBodies[i].tEntity);
        // ptSphereTransform->tTranslation = gptPhysicsCtx->sbtRigidBodies[i].tPosition;
        // ptSphereTransform->tScale = (plVec3){gptPhysicsCtx->sbtRigidBodies[i].fRadius, gptPhysicsCtx->sbtRigidBodies[i].fRadius, gptPhysicsCtx->sbtRigidBodies[i].fRadius};
        plVec3 tScale = {0};
        ptSphereTransform->tRotation = gptPhysicsCtx->sbtRigidBodies[i].tOrientation;
        ptSphereTransform->tTranslation = gptPhysicsCtx->sbtRigidBodies[i].tPosition;
        ptSphereTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
    }
}

void
pl_physics_reset(void)
{
    pl_sb_reset(gptPhysicsCtx->sbtRigidBodies);
}

void
pl_physics_draw(plComponentLibrary* ptLibrary, plDrawList3D* ptDrawlist)
{
    plEntity* sbtRigidBodyEntities = ptLibrary->tRigidBodyPhysicsComponentManager.sbtEntities;
    const uint32_t uRigidBodyCount = pl_sb_size(sbtRigidBodyEntities);
    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plEntity tEntity = sbtRigidBodyEntities[i];
        plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);

        if(ptBody->tShape == PL_COLLISION_SHAPE_SPHERE)
        {
            plDrawSphereDesc tDesc = {
                .fRadius = gptPhysicsCtx->sbtRigidBodies[i].fRadius,
                .tCenter = gptPhysicsCtx->sbtRigidBodies[i].tPosition
            };
            gptDraw->add_3d_sphere(ptDrawlist, tDesc, (plDrawLineOptions){.uColor = PL_COLOR_32_CYAN, .fThickness = 0.01f});
        }
        else if(ptBody->tShape == PL_COLLISION_SHAPE_BOX)
        {
            plVec3 tP0 = { 0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};
            plVec3 tP1 = { 0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y, -0.5f * ptBody->tExtents.z};
            plVec3 tP2 = {-0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y, -0.5f * ptBody->tExtents.z};
            plVec3 tP3 = {-0.5f * ptBody->tExtents.x, -0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};
            plVec3 tP4 = {  0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};
            plVec3 tP5 = {  0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  -0.5f * ptBody->tExtents.z};
            plVec3 tP6 = { -0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  -0.5f * ptBody->tExtents.z};
            plVec3 tP7 = { -0.5f * ptBody->tExtents.x, 0.5f * ptBody->tExtents.y,  0.5f * ptBody->tExtents.z};

            tP0 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP0);
            tP1 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP1);
            tP2 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP2);
            tP3 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP3);
            tP4 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP4);
            tP5 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP5);
            tP6 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP6);
            tP7 = pl_mul_mat4_vec3(&ptTransform->tWorld, tP7);


            gptDraw->add_3d_line(ptDrawlist, tP0, tP1, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP1, tP2, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP2, tP3, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP3, tP0, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});

            gptDraw->add_3d_line(ptDrawlist, tP4, tP5, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP5, tP6, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP6, tP7, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP7, tP4, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});

            gptDraw->add_3d_line(ptDrawlist, tP0, tP4, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP1, tP5, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP2, tP6, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
            gptDraw->add_3d_line(ptDrawlist, tP3, tP7, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.05f});
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
}

void
pl_physics_action(plComponentLibrary* ptLibrary)
{
    plEntity tEntity = {7, 0};

    plRigidBodyPhysicsComponent* ptBody = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_RIGID_BODY_PHYSICS, tEntity);
    plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);

    plRigidBody* ptRigidBody = &gptPhysicsCtx->sbtRigidBodies[ptBody->uPhysicsObject];

    plVec3 tForce = {0.0f, 10.0f, 0.0f};
    plVec3 tApplyPoint = {-1.2f, 0.0f, 0.0f};

    plVec3 tPoint = pl_sub_vec3(tApplyPoint, ptTransform->tTranslation);

    ptRigidBody->tForceAccumulation = pl_add_vec3(ptRigidBody->tForceAccumulation, tForce);

    plVec3 tTorque = {
        tPoint.y * tForce.z - tPoint.z * tForce.y,
        tPoint.z * tForce.x - tPoint.x * tForce.z,
        tPoint.x * tForce.y - tPoint.y * tForce.x
    };

    ptRigidBody->tTorqueAccumulation = pl_add_vec3(ptRigidBody->tTorqueAccumulation, tTorque);
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
    // find the distance from the origin
    float fBallDistance = pl_dot_vec3(ptPlane->tDirection, ptSphere->tTransform.col[3].xyz) - ptSphere->fRadius;

    // check for intersection
    return fBallDistance <= ptPlane->fOffset;
}

static bool
pl__intersect_sphere_sphere(const plCollisionPrimitive* ptSphere0, const plCollisionPrimitive* ptSphere1)
{
    // find the vector between the objects
    plVec3 tMidLine = pl_sub_vec3(ptSphere0->tTransform.col[3].xyz, ptSphere1->tTransform.col[3].xyz);

    // see if it is large enough
    return pl_length_sqr_vec3(tMidLine) < (ptSphere0->fRadius + ptSphere1->fRadius) * (ptSphere0->fRadius + ptSphere1->fRadius);
}


static void
pl__resolve_contacts(plContactResolver* ptResolver, float fDeltaTime)
{
    // prepare contacts for processing
    const uint32_t uContactCount = pl_sb_size(gptPhysicsCtx->tCollisionData.sbtContactArray);
    for(uint32_t i = 0; i < uContactCount; i++)
    {
        plContact* ptContact = &gptPhysicsCtx->tCollisionData.sbtContactArray[i];

        plRigidBody* ptBody0 = NULL;
        plRigidBody* ptBody1 = NULL;

        // check if first object is NULL and swap if it is
        if(ptContact->atBodyIndices[0] == UINT32_MAX)
        {
            ptContact->atBodyIndices[0] = ptContact->atBodyIndices[1];
            ptContact->atBodyIndices[1] = UINT32_MAX;
            
            ptBody0 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[0]];
        }

        if(ptContact->atBodyIndices[1] != UINT32_MAX)
            ptBody1 = &gptPhysicsCtx->sbtRigidBodies[ptContact->atBodyIndices[1]];

        // calculate a set of axis at the contact point
        calculateContactBasis();

        // store the relative position of the contact relative to each body
        ptContact->atRelativeContactPositions[0] = pl_sub_vec3(ptContact->tContactPoint, ptBody0->tPosition);
        if(ptBody1)
            ptContact->atRelativeContactPositions[1] = pl_sub_vec3(ptContact->tContactPoint, ptBody1->tPosition);

        // find the relative velocity of the bodies at the contact point
        // ptContact->tContactVelocity = 
    }


    // resolve the interpenetration problems with the contacts.

    // resolve the velocity problems with the contacts.
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
            plVec3 tAngularAcceleration = pl_mul_mat4_vec4(&ptBody->tInverseInertiaTensorWorld, (plVec4){.xyz = ptBody->tTorqueAccumulation}).xyz;

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
            tQ0 = pl_mul_quat(ptBody->tOrientation, tQ0);
            ptBody->tOrientation.x += tQ0.x * 0.5f;
            ptBody->tOrientation.y += tQ0.y * 0.5f;
            ptBody->tOrientation.z += tQ0.z * 0.5f;
            ptBody->tOrientation.w += tQ0.w * 0.5f;

            // Normalise the orientation, and update the matrices with the new
            // position and orientation
            ptBody->tOrientation = pl_norm_quat(ptBody->tOrientation);

            // calculate the transform matrix for the body.
            ptBody->tTransform = pl_rotation_translation_scale(ptBody->tOrientation, ptBody->tPosition, (plVec3){1.0f, 1.0f, 1.0f});

            ptBody->tInverseInertiaTensorWorld = pl_mul_mat4(&ptBody->tTransform, &ptBody->tInverseInertiaTensor);

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

                if (ptBody->fMotion < 0.1f)
                    ptBody->bIsAwake = false;
                else if (ptBody->fMotion > 10 * 0.1f)
                    ptBody->fMotion = 10 * 0.1f;
            }

        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_physics_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plPhysicsI tApi = {
        .initialize = pl_physics_initialize,
        .cleanup    = pl_physics_cleanup,
        .reset      = pl_physics_reset,
        .action     = pl_physics_action,
        .update     = pl_physics_update,
        .draw       = pl_physics_draw,
    };
    pl_set_api(ptApiRegistry, plPhysicsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory  = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptECS     = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptConsole = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptDraw    = pl_get_api_latest(ptApiRegistry, plDrawI);
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
