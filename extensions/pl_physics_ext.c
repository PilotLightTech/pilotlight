/*
   pl_physics_ext.c
*/

/*
Index of this file:
// [SECTION] notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global context
// [SECTION] internal api
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h>
#include "pl.h"
#include "pl_physics_ext.h"

// extensions
#include "pl_ecs_ext.h"
#include "pl_console_ext.h"

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
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plParticle
{
    plEntity tEntity;
    plVec3   tPosition;
    plVec3   tVelocity;
    plVec3   tAcceleration;
    float    fDamping;
    float    fInverseMass;
    plVec3   tForceAccumulation;
} plParticle;

typedef struct _plParticleContact
{
    uint64_t    auParticles[2];
    float       fRestitution;
    plVec3      tContactNormal;
    float       fPenetration;
    plVec3      atMovement[2];
} plParticleContact;

typedef struct _plPhysicsContext
{
    bool               bActive;
    plParticle*        sbtParticles;
    plParticleContact* sbtCollisions;
} plPhysicsContext;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plPhysicsContext* gptPhysicsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline float
pl__physics_calculate_separating_velocity(plParticleContact* ptContact)
{
    plVec3 tRelativeVelocity = gptPhysicsCtx->sbtParticles[ptContact->auParticles[0]].tVelocity;
    if(ptContact->auParticles[1] != UINT64_MAX)
    {
        tRelativeVelocity = pl_sub_vec3(tRelativeVelocity, gptPhysicsCtx->sbtParticles[ptContact->auParticles[1]].tVelocity);
    }
    return pl_dot_vec3(tRelativeVelocity, ptContact->tContactNormal);
}

static inline void
pl__physics_resolve_velocity(float fDeltaTime, plParticleContact* ptContact)
{
    float fSeparatingVelocity = pl__physics_calculate_separating_velocity(ptContact);

    // check if needs to be resolved
    if(fSeparatingVelocity > 0.0f)
    {
        // contact is either separating or stationary
        return;
    }

    plParticle* ptParticle0 = &gptPhysicsCtx->sbtParticles[ptContact->auParticles[0]];
    plParticle* ptParticle1 = ptContact->auParticles[1] == UINT64_MAX ? NULL : &gptPhysicsCtx->sbtParticles[ptContact->auParticles[1]];
    
    // calculate new separating velocity
    float fNewSeparatingVelocity = -fSeparatingVelocity * ptContact->fRestitution;

    // check the velocity buildup due to acceleration only
    plVec3 tAccCausedVelocity = ptParticle0->tAcceleration;
    if(ptParticle1)
        tAccCausedVelocity = pl_sub_vec3(tAccCausedVelocity, ptParticle1->tAcceleration);
    
    float fAccCausedSepVelocity = fDeltaTime * pl_dot_vec3(ptContact->tContactNormal, tAccCausedVelocity);

    // remove closing velocity built up from acceleration
    if(fAccCausedSepVelocity < 0.0f)
    {
        fNewSeparatingVelocity += ptContact->fRestitution * fAccCausedSepVelocity;

        // make sure we haven't removed more than possible
        if(fNewSeparatingVelocity < 0)
            fNewSeparatingVelocity = 0.0f;
    }

    float fDeltaVelocity = fNewSeparatingVelocity - fSeparatingVelocity;

    // apply change in velocity to each opbject in proportion to their inverse mass
    float fTotalInverseMass = ptParticle0->fInverseMass;
    if(ptParticle1)
        fTotalInverseMass += ptParticle1->fInverseMass;

    // if particles have no mass, then impulses have no effect
    if(fTotalInverseMass <= 0.0f)
        return;

    // calculate impulse to apply
    float fImpulse = fDeltaVelocity / fTotalInverseMass;

    // find amount of impulse per unit of inverse mass
    plVec3 tImpulsePerIMass = pl_mul_vec3_scalarf(ptContact->tContactNormal, fImpulse);

    // apply impulses
    ptParticle0->tVelocity = pl_add_vec3(ptParticle0->tVelocity, pl_mul_vec3_scalarf(tImpulsePerIMass, ptParticle0->fInverseMass));

    if(ptParticle1)
        ptParticle1->tVelocity = pl_add_vec3(ptParticle1->tVelocity, pl_mul_vec3_scalarf(tImpulsePerIMass, -ptParticle1->fInverseMass));
}

static inline void
pl__physics_resolve_interpenetration(float fDeltaTime, plParticleContact* ptContact)
{
    // skip if no penetration
    if(ptContact->fPenetration <= 0.0f)
        return;

    plParticle* ptParticle0 = &gptPhysicsCtx->sbtParticles[ptContact->auParticles[0]];
    plParticle* ptParticle1 = ptContact->auParticles[1] == UINT64_MAX ? NULL : &gptPhysicsCtx->sbtParticles[ptContact->auParticles[1]];
        
    // movement based on inverse mass
    float fTotalInverseMass = ptParticle0->fInverseMass;
    if(ptParticle1)
        fTotalInverseMass += ptParticle1->fInverseMass;

    // skip if infinite masses
    if(fTotalInverseMass <= 0.0f)
        return;

    // found amount of penetration resolution per unit of inverse mass
    plVec3 tMovePerIMass = pl_mul_vec3_scalarf(ptContact->tContactNormal, ptContact->fPenetration / fTotalInverseMass);

    // calculate movement amounts
    ptContact->atMovement[0] = pl_mul_vec3_scalarf(tMovePerIMass, ptParticle0->fInverseMass);
    
    if(ptParticle1)
        ptContact->atMovement[1] = pl_mul_vec3_scalarf(tMovePerIMass, -ptParticle1->fInverseMass);
    else
        ptContact->atMovement[1] = (plVec3){0};

    // apply penetration resolution
    ptParticle0->tPosition = pl_add_vec3(ptParticle0->tPosition, ptContact->atMovement[0]);
    if(ptParticle1)
        ptParticle1->tPosition = pl_add_vec3(ptParticle1->tPosition, ptContact->atMovement[1]);
}

static inline void
pl__physics_resolve(float fDeltaTime, plParticleContact* ptContact)
{
    pl__physics_resolve_velocity(fDeltaTime, ptContact);
    pl__physics_resolve_interpenetration(fDeltaTime, ptContact);
}

void
pl__physics_resolve_contacts(uint32_t uIterations, float fDeltaTime, plParticleContact* atContacts, uint32_t uContactCount)
{

    if(uContactCount == 0)
        return;

    uint32_t uIterationsUsed = 0;
    while(uIterationsUsed < uIterations)
    {
        float fMax = FLT_MAX;
        uint32_t uMaxIndex = uContactCount;
        for(uint32_t i = 0; i < uContactCount; i++)
        {
            float fSepVel = pl__physics_calculate_separating_velocity(&atContacts[i]);
            if(fSepVel < fMax && (fSepVel < 0 || atContacts[i].fPenetration > 0))
            {
                fMax = fSepVel;
                uMaxIndex = i;
            }
        }

        if(uMaxIndex == uContactCount)
            break;

        pl__physics_resolve(fDeltaTime, &atContacts[uMaxIndex]);

        // Update the interpenetrations for all particles
        plVec3* atMove = atContacts[uMaxIndex].atMovement;
        for (uint32_t i = 0; i < uContactCount; i++)
        {
            if (atContacts[i].auParticles[0] == atContacts[uMaxIndex].auParticles[0])
            {
                atContacts[i].fPenetration -= pl_dot_vec3(atMove[0], atContacts[i].tContactNormal);
            }
            else if (atContacts[i].auParticles[0] == atContacts[uMaxIndex].auParticles[1])
            {
                atContacts[i].fPenetration -= pl_dot_vec3(atMove[1], atContacts[i].tContactNormal);
            }
            if (atContacts[i].auParticles[1])
            {
                if (atContacts[i].auParticles[1] == atContacts[uMaxIndex].auParticles[0])
                {
                    atContacts[i].fPenetration += pl_dot_vec3(atMove[0], atContacts[i].tContactNormal);
                }
                else if (atContacts[i].auParticles[1] == atContacts[uMaxIndex].auParticles[1])
                {
                    atContacts[i].fPenetration += pl_dot_vec3(atMove[1], atContacts[i].tContactNormal);
                }
            }
        }

        uIterationsUsed++;
    }

}

void
pl__physics_integrate(float fDeltaTime, plParticle* atParticles, uint32_t uParticleCount)
{
    for(uint32_t i = 0; i < uParticleCount; i++)
    {
        if(atParticles[i].fInverseMass > 0.0f) // ignore particles with infinite mass
        {
            // update linear position
            atParticles[i].tPosition = pl_add_vec3(atParticles[i].tPosition, pl_mul_vec3_scalarf(atParticles[i].tVelocity, fDeltaTime));
            
            // work out acceleration from the fource
            plVec3 tResultingAcceleration = atParticles[i].tAcceleration;
            tResultingAcceleration = pl_add_vec3(tResultingAcceleration, pl_mul_vec3_scalarf(atParticles[i].tForceAccumulation, atParticles[i].fInverseMass));

            // atParticles[i].tPosition = pl_add_vec3(atParticles[i].tPosition, pl_mul_vec3_scalarf(tResultingAcceleration, fDeltaTime * fDeltaTime * 0.5f));

            // update linear velocity from the acceleration
            atParticles[i].tVelocity = pl_add_vec3(atParticles[i].tVelocity, pl_mul_vec3_scalarf(tResultingAcceleration, fDeltaTime));

            // impose drag
            atParticles[i].tVelocity = pl_mul_vec3_scalarf(atParticles[i].tVelocity, powf(atParticles[i].fDamping, fDeltaTime));

        }

        atParticles[i].tForceAccumulation = (plVec3){0};
    }
}

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
            ptRigidBody->uPhysicsObject = pl_sb_size(gptPhysicsCtx->sbtParticles);

            plParticle tParticle = {
                .fDamping      = 1.0f - ptRigidBody->fLinearDamping,
                .fInverseMass  = 1.0f / ptRigidBody->fMass,
                .tPosition     = ptTransform->tTranslation,
                .tAcceleration = ptRigidBody->tGravity,
                .tVelocity     = (plVec3){0},
                .tEntity       = tEntity
            };
            pl_sb_push(gptPhysicsCtx->sbtParticles, tParticle);
        }
        else
        {
            gptPhysicsCtx->sbtParticles[ptRigidBody->uPhysicsObject].fDamping = 1.0f - ptRigidBody->fLinearDamping;
            gptPhysicsCtx->sbtParticles[ptRigidBody->uPhysicsObject].fInverseMass = 1.0f / ptRigidBody->fMass;
            gptPhysicsCtx->sbtParticles[ptRigidBody->uPhysicsObject].tPosition = ptTransform->tTranslation;
            gptPhysicsCtx->sbtParticles[ptRigidBody->uPhysicsObject].tAcceleration = ptRigidBody->tGravity;
        }


    }

    // // bungee
    // // for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbtParticles); i++)
    // // {
    // //     gptDraw->add_3d_line(ptDrawlist, atAnchors[i], ptAppData->sbtParticles[i].tPosition, (plDrawLineOptions){.uColor = PL_COLOR_32_ORANGE, .fThickness = 0.05f});
    // //     plVec3 tForce = pl_sub_vec3(ptAppData->sbtParticles[i].tPosition, atAnchors[i]);

    // //     float fMagnitude = pl_length_vec3(tForce);
    // //     if(fMagnitude <= fSpringLength)
    // //         continue;

    // //     fMagnitude = (fSpringLength - fMagnitude) * fSpringConstant;

    // //     tForce = pl_norm_vec3(tForce);
    // //     tForce = pl_mul_vec3_scalarf(tForce, fMagnitude);
    // //     ptAppData->sbtParticles[i].tForceAccumulation = pl_add_vec3(ptAppData->sbtParticles[i].tForceAccumulation, tForce);
    // // }

    // // cable
    // for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbtParticles); i++)
    // {
    //     gptDraw->add_3d_line(ptDrawlist, atAnchors[i], ptAppData->sbtParticles[i].tPosition, (plDrawLineOptions){.uColor = PL_COLOR_32_ORANGE, .fThickness = 0.05f});
    //     plVec3 tLength = pl_sub_vec3(atAnchors[i], ptAppData->sbtParticles[i].tPosition);

    //     float fMagnitude = pl_length_vec3(tLength);
    //     if(fMagnitude < fSpringLength2)
    //         continue;

    //     plVec3 tNorm = pl_norm_vec3(tLength);

    //     plParticleContact tCollision = {
    //         .fPenetration = fMagnitude - fSpringLength2,
    //         .fRestitution = fSpringConstant,
    //         .tContactNormal = tNorm,
    //         .aptParticles = {
    //             &ptAppData->sbtParticles[i]
    //         }
    //     };
    //     pl_sb_push(ptAppData->sbtCollisions, tCollision); 
    // }

    // // rod
    // if(false)
    // {
    //     plVec3 tLength = pl_sub_vec3(ptAppData->sbtParticles[1].tPosition, ptAppData->sbtParticles[0].tPosition);
    //     gptDraw->add_3d_line(ptDrawlist, ptAppData->sbtParticles[1].tPosition, ptAppData->sbtParticles[0].tPosition, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.3f});
    //     float fMagnitude = pl_length_vec3(tLength);

    //     if(fMagnitude != fSpringLength)
    //     {

    //         plParticleContact tCollision = {
    //             .fRestitution = 0,
    //             .aptParticles = {
    //                 &ptAppData->sbtParticles[0],
    //                 &ptAppData->sbtParticles[1]
    //             }
    //         };

    //         plVec3 tNorm = pl_norm_vec3(tLength);

    //         if(fMagnitude > fSpringLength)
    //         {
    //             tCollision.tContactNormal = tNorm;
    //             tCollision.fPenetration = fMagnitude - fSpringLength;
    //         }
    //         else
    //         {
    //             tCollision.tContactNormal = pl_mul_vec3_scalarf(tNorm, -1.0f);
    //             tCollision.fPenetration = fSpringLength - fMagnitude;
                
    //         }

    //         pl_sb_push(ptAppData->sbtCollisions, tCollision);
    //     }

    // }

    // others
    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plParticle* ptParticle0 = &gptPhysicsCtx->sbtParticles[i];
        for(uint32_t j = 0; j < uRigidBodyCount; j++)
        {
            if(i == j)
                continue;

            plParticle* ptParticle1 = &gptPhysicsCtx->sbtParticles[j];

            plVec3 tDifference = pl_sub_vec3(ptParticle0->tPosition, ptParticle1->tPosition);
            float fPenetration = fabsf(pl_length_vec3(tDifference));

            if(fPenetration < 1.0f)
            {
                plParticleContact tCollision = {
                    .fPenetration = 1.0f - fPenetration,
                    .fRestitution = 0.2f,
                    .tContactNormal = pl_norm_vec3(tDifference),
                    .auParticles = {
                        (uint64_t)i,
                        (uint64_t)j
                    }
                };
                pl_sb_push(gptPhysicsCtx->sbtCollisions, tCollision); 
            }
        }
    }

    // ground
    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        float fPenetration = 0.5f - gptPhysicsCtx->sbtParticles[i].tPosition.y;

        if(fabsf(gptPhysicsCtx->sbtParticles[i].tPosition.x) > 5.0f)
            continue;

        if(fabsf(gptPhysicsCtx->sbtParticles[i].tPosition.z) > 5.0f)
            continue;

        if(fPenetration > 0.001f)
        {
            plParticleContact tCollision = {
                .fPenetration = fPenetration,
                .fRestitution = 0.2f,
                .tContactNormal = {0.0f, 1.0f, 0.0f},
                .auParticles = {
                    (uint64_t)i,
                    UINT64_MAX
                }
            };
            pl_sb_push(gptPhysicsCtx->sbtCollisions, tCollision);
        }
    }

    pl__physics_integrate(fDeltaTime, gptPhysicsCtx->sbtParticles, uRigidBodyCount);
    pl__physics_resolve_contacts(pl_sb_size(gptPhysicsCtx->sbtCollisions) * 2, fDeltaTime, gptPhysicsCtx->sbtCollisions, pl_sb_size(gptPhysicsCtx->sbtCollisions));
    pl_sb_reset(gptPhysicsCtx->sbtCollisions);

    for(uint32_t i = 0; i < uRigidBodyCount; i++)
    {
        plTransformComponent* ptSphereTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, gptPhysicsCtx->sbtParticles[i].tEntity);
        ptSphereTransform->tTranslation = gptPhysicsCtx->sbtParticles[i].tPosition;
        ptSphereTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
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
        .update     = pl_physics_update
    };
    pl_set_api(ptApiRegistry, plPhysicsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory  = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptECS     = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptConsole = pl_get_api_latest(ptApiRegistry, plConsoleI);
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

    pl_sb_free(gptPhysicsCtx->sbtCollisions);
    pl_sb_free(gptPhysicsCtx->sbtParticles);
}
